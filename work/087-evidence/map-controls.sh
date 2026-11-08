#!/usr/bin/env bash
# Unit 087's map controls: libbid's per-function sections in each build's link map (run from the repo root, after
# scripts/resources.py static 86b3fbf 0a7de57 has built both under build/res/).
#   sections: input sections from libbid.a whose name has a third part (.text.<fn>, .literal.<fn>, .rodata.<name>),
#             counted from the map's two-line form (the section name on the line before the libbid.a file);
#   .text.__bid128_add: the one named function, present only when the library was built with -ffunction-sections.
# The base's one entry is .rodata.str1.4, the compiler's merged strings, emitted with or without the flags.
for c in 86b3fbf 0a7de57; do for t in reva standin; do
    M=build/res/$c/device/$t/build-res/abacus_$t.map
    n=$(grep -B1 'libbid.a(' "$M" | grep -c -E '^ \.(text|literal|rodata)\.')
    a=$(grep -c -E '^ \.text\.__bid128_add( |$)' "$M")
    printf '%s %-8s libbid sections %4s  .text.__bid128_add %s  (%s)\n' "$c" "$t" "$n" "$a" \
        "$(grep -B1 'libbid.a(' "$M" | grep -E '^ \.(text|literal|rodata)\.' | sort -u | head -3 | tr -d ' ' | tr '\n' ' ')"
done; done
for c in 86b3fbf 0a7de57; do echo "$c flag stamp: $(cat build/res/$c/third_party/intel-dfp/esp32s3/IntelRDFPMathLib20U5/.abacus-flags 2>/dev/null | grep -o 'CFLAGS_OPT=[^C]*')"; done
