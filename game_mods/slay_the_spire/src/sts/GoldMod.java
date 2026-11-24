package sts;

import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.math.Vector2;
import com.badlogic.gdx.utils.viewport.FitViewport;
import com.megacrit.cardcrawl.core.CardCrawlGame;
import com.megacrit.cardcrawl.core.Settings;
import com.megacrit.cardcrawl.helpers.input.InputHelper;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.PrintWriter;

public final class GoldMod {
    private static volatile double multiplier = 3.0;
    private static boolean initialized = false;
    private static final Vector2 TMP_MOUSE = new Vector2();

    private static synchronized void init() {
        if (initialized) return;
        initialized = true;

        File ini = findConfigFile();
        String source = "default";
        if (ini != null && ini.isFile()) {
            source = ini.getAbsolutePath();
            try (BufferedReader br = new BufferedReader(new FileReader(ini))) {
                String line;
                while ((line = br.readLine()) != null) {
                    line = line.trim();
                    if (line.isEmpty() || line.startsWith("#") || line.startsWith(";") || line.startsWith("[")) {
                        continue;
                    }
                    int eq = line.indexOf('=');
                    if (eq <= 0) continue;
                    String key = line.substring(0, eq).trim();
                    String val = line.substring(eq + 1).trim();
                    if ("gold_multiplier".equalsIgnoreCase(key)) {
                        double parsed = Double.parseDouble(val);
                        if (parsed > 0.0 && parsed <= 1000.0) {
                            multiplier = parsed;
                        }
                    }
                }
            } catch (Throwable ignored) {
            }
        }

        try {
            File logFile = (ini != null && ini.getParentFile() != null)
                ? new File(ini.getParentFile(), "sts_gold.log")
                : new File("sts_gold.log");
            try (PrintWriter pw = new PrintWriter(new FileWriter(logFile, true))) {
                pw.println("[StS GoldMod] Loaded gold_multiplier=" + multiplier + " (from " + source + ")");
            }
        } catch (Throwable ignored) {
        }
    }

    private static File findConfigFile() {
        File cwd = new File("sts_gold.ini");
        if (cwd.isFile()) return cwd;
        try {
            File loc = new File(GoldMod.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            File dir = loc.isDirectory() ? loc : loc.getParentFile();
            if (dir != null) {
                File f = new File(dir, "sts_gold.ini");
                if (f.isFile()) return f;
            }
        } catch (Throwable ignored) {
        }
        return cwd;
    }

    public static int scale(int amount) {
        if (!initialized) init();
        if (amount <= 0) return amount;
        long scaled = Math.round((double) amount * multiplier);
        if (scaled < 1L) return 1;
        if (scaled > Integer.MAX_VALUE) return Integer.MAX_VALUE;
        return (int) scaled;
    }

    public static void onResize(int width, int height) {
        if (width <= 0 || height <= 0) return;
        Settings.M_W = width;
        Settings.M_H = height;
        FitViewport vp = CardCrawlGame.viewport;
        if (vp != null) {
            vp.update(width, height, false);
        }
    }

    public static void fixMouse() {
        FitViewport vp = CardCrawlGame.viewport;
        if (vp == null || Gdx.input == null) return;
        int sw = vp.getScreenWidth();
        int sh = vp.getScreenHeight();
        if (sw <= 0 || sh <= 0) return;
        if (sw == Settings.WIDTH && sh == Settings.HEIGHT
                && vp.getScreenX() == 0 && vp.getScreenY() == 0) {
            return;
        }
        TMP_MOUSE.set((float) Gdx.input.getX(), (float) Gdx.input.getY());
        vp.unproject(TMP_MOUSE);
        int x = Math.round(TMP_MOUSE.x);
        int y = Math.round(TMP_MOUSE.y);
        if (x < 0) x = 0;
        else if (x > Settings.WIDTH) x = Settings.WIDTH;
        if (y < 1) y = 1;
        else if (y > Settings.HEIGHT) y = Settings.HEIGHT;
        InputHelper.mX = x;
        InputHelper.mY = y;
    }
}
