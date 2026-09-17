# Native Arc deposit eligibility evidence

Pinned game image: `8713D15E3D05B26F9E259E02B0F29BC1E000E4B0C62CA2CC87C38597186CC3BD`.

`charge-property.bin` is installed package component `80F7A6E0`; `other-property.bin` is `80FEF337`. Both are `80809C36` package files. Read-only inspection of PID 7320 while carrying the charge found both providers on player `50FAA3C4` through the native group chain. The former was on group `32F9E29E`, entity definition `80F58EA5`, at component offset `70`. The latter was on group `43F9E468`, entity definition `80C1934E`, at offset `70`. Both register query interface `80803B37`, reference method table `815B887B`, and getter `C994A0`. Their definition offset is `620`; their named property at definition `+9C` is respectively `9C99BE55` and `9CBEE071`. Their lookup-row exclusion masks were zero.

The actual native registry resolves predicate `80804D75` to `100BF10 -> 100C000`. This enumerates the actor's `80803B37` providers through `591110`, invokes `184FD70`, and accepts any provider whose getter returns true. The getter only compares the authored property hash; it is not a host mission-state flag. The separate `80804D71` actor filter has not been fully replayed here.

`sink-definition.bin` is installed package component `80F6666E`. `sink-live-7320.bin` is the read-only captured instance from `arc-live-7320-1788660239`, controller `4EF9E33C`, entity `1FFAA227`. Requested and consumed counters are zero. Original `F30540` tests the configured one-shot/toggle mode, used state and outstanding counters. The fixture executes it on this actual capture and checks pending/completed negative cases.

Tests execute original `C994A0` and `F30540` in the isolated unit process. Provider definition references are modeled from the captured enumeration, with actual package bytes. These tests establish the carried property's match and sink request readiness. They do not prove prompt selection, input routing, interaction animation, consumption, or post-dunk transport. No game process is called or written by the fixture.
