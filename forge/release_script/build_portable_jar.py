#!/usr/bin/env python3
"""
Builds a self-contained, platform-independent executable JAR (NeoForge.jar)
from NeoForge/forge-gui-neo and the official release bundle in release/,
WITHOUT modifying any files in NeoForge/.
"""

import datetime
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from collections import defaultdict
from pathlib import Path


MAVEN_CENTRAL = "https://repo1.maven.org/maven2"

ADD_OPENS_MODULES = [
    "java.base/java.util",
    "java.base/java.lang",
    "java.base/java.lang.reflect",
    "java.base/java.text",
    "java.base/java.util.concurrent",
    "java.base/java.math",
    "java.base/java.net",
    "java.base/java.nio",
    "java.base/sun.nio.ch",
    "java.base/jdk.internal.misc",
    "java.desktop/java.beans",
    "java.desktop/javax.swing.border",
    "java.desktop/javax.swing.event",
    "java.desktop/sun.swing",
    "java.desktop/java.awt.image",
    "java.desktop/java.awt.color",
    "java.desktop/sun.awt.image",
    "java.desktop/javax.swing",
    "java.desktop/java.awt",
    "java.desktop/java.awt.font",
]

BOOTSTRAP_JAVA_SOURCE = r"""package forge.neo;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.Enumeration;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

/**
 * Platform-independent bootstrap launcher for the standalone NeoForge.jar.
 * Extracts embedded game assets (res/) beside the JAR on first launch if needed,
 * configures portable paths, and delegates to NeoMain.
 */
public final class NeoJarBootstrap {

    private static final String RES_PREFIX = "META-INF/neoforge-res/";

    private NeoJarBootstrap() {
    }

    public static void main(final String[] args) throws Exception {
        final File jarFile = locateJarFile();
        final File baseDir = (jarFile != null && jarFile.getParentFile() != null)
                ? jarFile.getParentFile().toPath().toAbsolutePath().normalize().toFile()
                : new File(".").toPath().toAbsolutePath().normalize().toFile();

        if (System.getProperty("file.encoding") == null) {
            System.setProperty("file.encoding", "UTF-8");
        }

        final String[] effectiveArgs = (args == null || args.length == 0)
                ? new String[] { "ui" }
                : args;

        ensureAssetsExtracted(jarFile, baseDir);

        if (System.getProperty("forge.assetsDir") == null) {
            final File localRes = new File(baseDir, "res");
            if (localRes.isDirectory()) {
                System.setProperty("forge.assetsDir", baseDir.getAbsolutePath() + File.separator);
            }
        }

        if (System.getProperty("neo.dataDir") == null
                && !(effectiveArgs.length > 0 && "portablecheck".equalsIgnoreCase(effectiveArgs[0]))) {
            final File datosDir = new File(baseDir, "datos");
            System.setProperty("neo.dataDir", datosDir.getAbsolutePath());
        }

        NeoMain.main(effectiveArgs);
    }

    private static File locateJarFile() {
        try {
            final URL url = NeoJarBootstrap.class.getProtectionDomain().getCodeSource().getLocation();
            if (url != null) {
                final File f = new File(url.toURI());
                if (f.isFile()) {
                    return f;
                }
            }
        } catch (final URISyntaxException | IllegalArgumentException ignored) {
        }
        return null;
    }

    private static void ensureAssetsExtracted(final File jarFile, final File baseDir) {
        final File resDir = new File(baseDir, "res");
        final File cardsZip = new File(new File(resDir, "cardsfolder"), "cardsfolder.zip");
        if (cardsZip.isFile() || jarFile == null || !jarFile.isFile()) {
            return;
        }

        System.out.println("[NeoForge] First launch: extracting game assets (res/) to " + resDir + " ...");
        final byte[] buf = new byte[65536];
        int count = 0;
        try (JarFile jf = new JarFile(jarFile)) {
            final Enumeration<JarEntry> entries = jf.entries();
            while (entries.hasMoreElements()) {
                final JarEntry entry = entries.nextElement();
                final String name = entry.getName();
                if (!name.startsWith(RES_PREFIX)) {
                    continue;
                }
                final String rel = name.substring(RES_PREFIX.length());
                if (rel.isEmpty()) {
                    continue;
                }
                final File out = new File(resDir, rel.replace('/', File.separatorChar));
                if (entry.isDirectory()) {
                    out.mkdirs();
                    continue;
                }
                final File parent = out.getParentFile();
                if (parent != null && !parent.isDirectory()) {
                    parent.mkdirs();
                }
                try (InputStream in = jf.getInputStream(entry);
                     OutputStream os = new FileOutputStream(out)) {
                    int n;
                    while ((n = in.read(buf)) >= 0) {
                        os.write(buf, 0, n);
                    }
                }
                count++;
            }
            System.out.println("[NeoForge] Extracted " + count + " asset files.");
        } catch (final IOException e) {
            System.err.println("[NeoForge] Failed to extract embedded assets: " + e);
        }
    }
}
"""


def find_jdk_bin() -> Path:
    candidates = []
    if os.environ.get("JAVA_HOME"):
        candidates.append(Path(os.environ["JAVA_HOME"]) / "bin")
    which_javac = shutil.which("javac")
    if which_javac:
        candidates.append(Path(which_javac).resolve().parent)
    candidates.extend([
        Path("/home/chih/Tools/android-studio/jbr/bin"),
        Path("/usr/lib/jvm/default-java/bin"),
    ])
    for jvm_dir in sorted(Path("/usr/lib/jvm").glob("java-*"), reverse=True):
        candidates.append(jvm_dir / "bin")

    for bin_dir in candidates:
        if (bin_dir / "javac").is_file() and (bin_dir / "java").is_file():
            return bin_dir
    raise RuntimeError("Could not find a JDK (javac/java). Set JAVA_HOME to a JDK 17+ installation.")


def read_pom_property(pom_text: str, tag: str, default: str = "") -> str:
    m = re.search(rf"<{re.escape(tag)}>\s*([^<\s]+)\s*</{re.escape(tag)}>", pom_text)
    return m.group(1) if m else default


def ensure_cached_javafx_jars(cache_dir: Path, jfx_version: str) -> list[Path]:
    cache_dir.mkdir(parents=True, exist_ok=True)
    artifacts = ["javafx-base", "javafx-controls", "javafx-graphics", "javafx-media"]
    platforms = ["linux", "mac"]
    paths = []
    for plat in platforms:
        for art in artifacts:
            fname = f"{art}-{jfx_version}-{plat}.jar"
            dest = cache_dir / fname
            if not dest.is_file() or dest.stat().st_size == 0:
                url = f"{MAVEN_CENTRAL}/org/openjfx/{art}/{jfx_version}/{fname}"
                print(f"  Downloading {fname} from Maven Central...")
                with urllib.request.urlopen(url) as resp:
                    data = resp.read()
                dest.write_bytes(data)
            paths.append(dest)
    return paths


def make_manifest() -> bytes:
    lines = [
        "Manifest-Version: 1.0",
        "Main-Class: forge.neo.NeoJarBootstrap",
        "Add-Opens: " + " ".join(ADD_OPENS_MODULES),
    ]
    wrapped = []
    for line in lines:
        encoded = line.encode("utf-8")
        if len(encoded) <= 70:
            wrapped.append(line)
        else:
            chunk = line[:70]
            rest = line[70:]
            wrapped.append(chunk)
            while rest:
                wrapped.append(" " + rest[:69])
                rest = rest[69:]
    return ("\r\n".join(wrapped) + "\r\n\r\n").encode("utf-8")


def should_skip_jar_entry(name: str) -> bool:
    if name.endswith("/"):
        return True
    norm = name.upper()
    if norm == "META-INF/MANIFEST.MF":
        return True
    if norm == "MODULE-INFO.CLASS" or norm.endswith("/MODULE-INFO.CLASS"):
        return True
    if norm.startswith("META-INF/") and (
        norm.endswith(".SF") or norm.endswith(".DSA") or norm.endswith(".RSA") or norm.endswith(".EC")
    ):
        return True
    if norm == "META-INF/INDEX.LIST":
        return True
    return False


def main() -> int:
    forge_root = Path(__file__).resolve().parent.parent
    neoforge_repo = forge_root / "NeoForge"
    module_dir = neoforge_repo / "forge-gui-neo"
    release_dir = forge_root / "release"
    win_zip_path = release_dir / "NeoForge-windows-x64.zip"
    cache_dir = Path(__file__).resolve().parent / ".cache"

    if not module_dir.is_dir():
        raise FileNotFoundError(f"Missing NeoForge module directory: {module_dir}")
    if not win_zip_path.is_file():
        raise FileNotFoundError(f"Missing official release zip: {win_zip_path}")

    pom_text = (module_dir / "pom.xml").read_text(encoding="utf-8")
    neo_version = read_pom_property(pom_text, "neo.version", "1.0.56")
    jfx_version = read_pom_property(pom_text, "javafx.version", "21.0.5")

    out_jar_name = os.environ.get("OUT_JAR_NAME", "NeoForge.jar")
    out_jar_path = Path(os.environ.get("OUT_JAR", str(release_dir / out_jar_name))).resolve()

    jdk_bin = find_jdk_bin()
    javac_bin = jdk_bin / "javac"
    print(f"=== Building Platform-Independent NeoForge {neo_version} JAR ===")
    print(f"  Using JDK: {jdk_bin}")
    print(f"  Source module: {module_dir}")
    print(f"  Base release bundle: {win_zip_path}")
    print(f"  Output JAR: {out_jar_path}")

    print("[1/4] Fetching cross-platform JavaFX libraries (Linux & macOS)...")
    extra_jfx_jars = ensure_cached_javafx_jars(cache_dir, jfx_version)

    with tempfile.TemporaryDirectory(prefix="neoforge_build_") as tmp:
        tmp_path = Path(tmp)
        deps_dir = tmp_path / "deps"
        classes_dir = tmp_path / "classes"
        boot_src_dir = tmp_path / "boot_src"
        deps_dir.mkdir()
        classes_dir.mkdir()
        boot_src_dir.mkdir()

        print("[2/4] Extracting engine and library JARs from release bundle...")
        forge_version = "2.0.17-SNAPSHOT"
        dep_jars: list[Path] = []
        with zipfile.ZipFile(win_zip_path, "r") as wz:
            for info in wz.infolist():
                if info.filename.startswith("NeoForge/app/") and info.filename.endswith(".jar"):
                    jar_name = Path(info.filename).name
                    if jar_name == "neo.jar":
                        continue
                    m = re.match(r"forge-core-(.+)\.jar$", jar_name)
                    if m:
                        forge_version = m.group(1)
                    target_jar = deps_dir / jar_name
                    target_jar.write_bytes(wz.read(info.filename))
                    dep_jars.append(target_jar)

        print(f"[3/4] Compiling forge-gui-neo ({neo_version}) from source with javac...")
        java_sources = sorted(str(p) for p in (module_dir / "src/main/java").rglob("*.java"))
        boot_java = boot_src_dir / "forge/neo/NeoJarBootstrap.java"
        boot_java.parent.mkdir(parents=True, exist_ok=True)
        boot_java.write_text(BOOTSTRAP_JAVA_SOURCE, encoding="utf-8")
        java_sources.append(str(boot_java))

        cp_sep = ";" if os.name == "nt" else ":"
        classpath = cp_sep.join(str(p) for p in dep_jars)
        argfile = tmp_path / "sources.txt"
        argfile.write_text("\n".join(f'"{s}"' for s in java_sources), encoding="utf-8")

        cmd = [
            str(javac_bin),
            "--release", "17",
            "-encoding", "UTF-8",
            "-g",
            "-cp", classpath,
            "-d", str(classes_dir),
            f"@{argfile}",
        ]
        subprocess.run(cmd, check=True)

        # Copy resources from src/main/resources and filter build.properties
        res_src_dir = module_dir / "src/main/resources"
        build_date = datetime.date.today().strftime("%Y-%m-%d")
        for res_file in res_src_dir.rglob("*"):
            if not res_file.is_file():
                continue
            rel = res_file.relative_to(res_src_dir)
            dest_file = classes_dir / rel
            dest_file.parent.mkdir(parents=True, exist_ok=True)
            if rel.as_posix() == "forge/neo/build.properties":
                raw = res_file.read_text(encoding="utf-8")
                filtered = (
                    raw.replace("${neo.version}", neo_version)
                    .replace("${project.version}", forge_version)
                    .replace("${neo.buildTime}", build_date)
                )
                dest_file.write_text(filtered, encoding="utf-8")
            else:
                shutil.copy2(res_file, dest_file)

        print("[4/4] Packaging platform-independent fat JAR with embedded assets...")
        tmp_out_jar = tmp_path / "NeoForge.jar"
        seen_entries: set[str] = set()
        services: dict[str, list[str]] = defaultdict(list)

        with zipfile.ZipFile(tmp_out_jar, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=1) as out_zf:
            # 1. Manifest
            out_zf.writestr("META-INF/MANIFEST.MF", make_manifest())
            seen_entries.add("META-INF/MANIFEST.MF")

            # 2. Compiled forge-gui-neo classes & resources (highest priority)
            for cf in sorted(classes_dir.rglob("*")):
                if not cf.is_file():
                    continue
                arcname = cf.relative_to(classes_dir).as_posix()
                if arcname not in seen_entries:
                    out_zf.write(cf, arcname)
                    seen_entries.add(arcname)

            # 3. All dependency JARs + Linux/macOS JavaFX JARs
            all_jars = dep_jars + extra_jfx_jars
            for jpath in all_jars:
                with zipfile.ZipFile(jpath, "r") as jzf:
                    for jinfo in jzf.infolist():
                        ename = jinfo.filename
                        if should_skip_jar_entry(ename):
                            continue
                        if ename.startswith("META-INF/services/"):
                            content = jzf.read(ename).decode("utf-8", errors="replace").strip()
                            if content and content not in services[ename]:
                                services[ename].append(content)
                            continue
                        if ename in seen_entries:
                            continue
                        out_zf.writestr(ename, jzf.read(ename))
                        seen_entries.add(ename)

            # 4. Merged META-INF/services/*
            for sname, svalues in sorted(services.items()):
                out_zf.writestr(sname, ("\n".join(svalues) + "\n").encode("utf-8"))
                seen_entries.add(sname)

            # 5. Embed NeoForge/res/* under META-INF/neoforge-res/* so the JAR is 100% self-contained
            with zipfile.ZipFile(win_zip_path, "r") as wz:
                for info in wz.infolist():
                    if not info.filename.startswith("NeoForge/res/") or info.is_dir():
                        continue
                    rel_res = info.filename[len("NeoForge/res/"):]
                    if not rel_res:
                        continue
                    arcname = f"META-INF/neoforge-res/{rel_res}"
                    # Store already-compressed files (like cardsfolder.zip, .mp3, .png) without re-deflating
                    if rel_res.endswith((".zip", ".png", ".jpg", ".mp3", ".ogg", ".wav")):
                        out_zf.writestr(arcname, wz.read(info.filename), compress_type=zipfile.ZIP_STORED)
                    else:
                        out_zf.writestr(arcname, wz.read(info.filename))

        out_jar_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(tmp_out_jar), str(out_jar_path))

    size_mb = out_jar_path.stat().st_size / (1024 * 1024)
    print(f"=== Successfully built {out_jar_path} ({size_mb:.1f} MB) ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
