# decomp.me workflow

The available Nintendo DS decomp.me platform is `nds_arm9` (ARMv5TE/arm32). It has no first-class ARM7, overlay, ROM-link, or module model. Use it for one sanitized ARM9 function at a time.

## Local preparation

`tools/decompme.py` accepts a target assembly file and emits a JSON package containing only the selected function, mode directives, relocation labels, literal-pool context, compiler ID, flags, and an optional diff label. It rejects paths outside the repository's local scratch area unless explicitly given a single target assembly file.

Example:

```sh
python tools/decompme.py prepare \
  --assembly scratch/AWRP00/func_02012340.s \
  --function func_02012340 \
  --compiler mwcc_20_56 \
  --flags '-proc arm9 -enum int -O4,p' \
  --output scratch/AWRP00/func_02012340.decompme.json
```

The output is a sanitized handoff aid, not a build artifact and not a replacement for local linking.

## Remote scratch rules

1. Create a scratch through decomp.me `/new` using the chosen `nds_arm9` compiler ID.
2. Compile through `/api/scratch/<slug>/compile` only after reviewing the payload.
3. Export only the source or target-only function data needed for analysis through `/api/scratch/<slug>/export?target_only=1`.
4. Treat assembly, context, compiler metadata, and links as public.
5. Never upload the ROM, a full overlay, filesystem/assets, target dumps, repository secrets, OAuth/session files, proprietary compiler archives, or ARM7 material.

Import only source and flag lessons. Rebuild the object locally and require `objdiff` byte/relocation/data equality before claiming a match.
