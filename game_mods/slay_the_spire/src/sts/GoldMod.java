package sts;

import com.megacrit.cardcrawl.characters.AbstractPlayer;
import com.megacrit.cardcrawl.potions.PotionSlot;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.PrintWriter;

public final class GoldMod {
    public static final String MOD_VERSION = "1.2";
    private static volatile double goldMultiplier = 3.0;
    private static volatile int startingPotionSlots = 5;
    private static volatile int cardsPerTurn = 6;
    private static boolean initialized = false;

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
                            goldMultiplier = parsed;
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
                ? new File(ini.getParentFile(), "sts_gold.log")
                : new File("sts_gold.log");
            try (PrintWriter pw = new PrintWriter(new FileWriter(logFile, true))) {
                pw.println("[StS Mod v" + MOD_VERSION + "] gold_multiplier=" + goldMultiplier
                    + ", starting_potion_slots=" + startingPotionSlots
                    + ", cards_per_turn=" + cardsPerTurn
                    + " (from " + source + ")");
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
        long scaled = Math.round((double) amount * goldMultiplier);
        if (scaled < 1L) return 1;
        if (scaled > Integer.MAX_VALUE) return Integer.MAX_VALUE;
        return (int) scaled;
    }

    public static void onInitPlayer(AbstractPlayer p) {
        if (!initialized) init();
        if (p == null) return;

        // Starting cards drawn per turn (vanilla = 5)
        if (cardsPerTurn > 0) {
            p.masterHandSize = cardsPerTurn;
            p.gameHandSize = cardsPerTurn;
        }

        // Starting potion slots (vanilla = 3)
        if (startingPotionSlots > 0 && p.potions != null) {
            p.potionSlots = startingPotionSlots;
            p.potions.clear();
            for (int i = 0; i < startingPotionSlots; i++) {
                p.potions.add(new PotionSlot(i));
            }
            p.adjustPotionPositions();
        }
    }
}
