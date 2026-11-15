This is a 2D ASCII based soul-like game.

The player and enemies are represented by ASCII symbols, but not a single symbol, but much bigger, maybe occupying most of an 5x5 area or even larger (e.g. enemy boss).

The player and enemies have 4-direction facing.

The player and enemies can attack with their equipped weapons, which shows an ASCII based animation. The game is real time but the attack animation cannot be too fast since ASCII animation won't be as accurate.

The player can be moved by WASD and use two hands' equipment using UIJK. U and J are for left hand, I and K for right hand, and J and K for "normal" use and U and I for "special". (J and K would be for "light attack" in soul games.)

Both player's and enemies hitbox part of the weapon / equipment are shown with a different color to make them more noticeable.

As the starting point, make:
- 1 enemy type: skeleton, which is slow in action, and wield a single hand sword
- The player, which wield a shield on left hand (J for block, timely block for parry, U is not used), and a spear on the right hand (K for fast, less damage attack, I for slower, more powerful attack)
- Make sure their ASCII art can tell which way they are facing, and they always attack to the direction they face (but the animation / hitbox doesn't have to be completely in that direction, for example it could be a swing that attacks the side as well)
- The player can roll in the direction it's facing with L key, maybe with simple ASCII animation, or no animation if that's a lot easier (simulating stepping)
- Spawn 2 ememies and the player, so I can play it and get a sense of how good it is

