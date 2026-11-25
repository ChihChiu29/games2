package sts;

import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.math.Vector2;
import com.badlogic.gdx.utils.viewport.FitViewport;
import com.megacrit.cardcrawl.characters.AbstractPlayer;
import com.megacrit.cardcrawl.core.CardCrawlGame;
import com.megacrit.cardcrawl.core.Settings;
import com.megacrit.cardcrawl.helpers.input.InputHelper;
import com.megacrit.cardcrawl.potions.PotionSlot;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.PrintWriter;

public final class GoldMod {
    public static final String MOD_VERSION = "1.3";
    private static volatile double multiplier = 3.0;
    private static volatile int startingPotionSlots = 5;
    private static volatile int cardsPerTurn = 6;
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
                    } else if ("starting_potion_slots".equalsIgnoreCase(key)) {
                        int parsed = Integer.parseInt(val);
                        if (parsed >= 1 && parsed <= 12) {
                            startingPotionSlots = parsed;
                        }
                    } else if ("cards_per_turn".equalsIgnoreCase(key)) {
                        int parsed = Integer.parseInt(val);
                        if (parsed >= 1 && parsed <= 10) {
                            cardsPerTurn = parsed;
                        }
                    }
                }
            } catch (Throwable ignored) {
            }
        }

        try {
            File logFile = (ini != null && ini.getParentFile() != null)
                ? new File(ini.getParentFile(), "sts_cheat.log")
                : new File("sts_cheat.log");
            try (PrintWriter pw = new PrintWriter(new FileWriter(logFile, true))) {
                pw.println("[StS Mod v" + MOD_VERSION + "] gold_multiplier=" + multiplier
                    + ", starting_potion_slots=" + startingPotionSlots
                    + ", cards_per_turn=" + cardsPerTurn
                    + " (from " + source + ")");
            }
        } catch (Throwable ignored) {
        }
    }

    private static File findConfigFile() {
        String[] names = new String[] { "sts_cheat.ini", "sts_gold.ini" };
        for (String name : names) {
            File cwd = new File(name);
            if (cwd.isFile()) return cwd;
        }
        try {
            File loc = new File(GoldMod.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            File dir = loc.isDirectory() ? loc : loc.getParentFile();
            if (dir != null) {
                for (String name : names) {
                    File f = new File(dir, name);
                    if (f.isFile()) return f;
                }
            }
        } catch (Throwable ignored) {
        }
        return new File("sts_cheat.ini");
    }

    public static int scale(int amount) {
        if (!initialized) init();
        if (amount <= 0) return amount;
        long scaled = Math.round((double) amount * multiplier);
        if (scaled < 1L) return 1;
        if (scaled > Integer.MAX_VALUE) return Integer.MAX_VALUE;
        return (int) scaled;
    }

    public static void onInitPlayer(AbstractPlayer p) {
        if (!initialized) init();
        if (p == null) return;

        if (cardsPerTurn > 0) {
            p.masterHandSize = cardsPerTurn;
            p.gameHandSize = cardsPerTurn;
        }

        if (startingPotionSlots > 0 && p.potions != null) {
            p.potionSlots = startingPotionSlots;
            p.potions.clear();
            for (int i = 0; i < startingPotionSlots; i++) {
                p.potions.add(new PotionSlot(i));
            }
            p.adjustPotionPositions();
        }
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
