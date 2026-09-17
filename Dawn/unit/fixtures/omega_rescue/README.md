# Osiris rescue regression evidence

The 360/369/185 component, object, and authority files are the read-only PID 44932 capture from 2026-09-05. They reproduce decoded NPC/Scene/route authority not adopted by live components.

`80EC0DCF`, `80EC0DD4`, `80EC0DCA`, and `80EC0DFF` are extracted packaged graph assets. Their reciprocal headers distinguish runtime classes 80806384/808084E9 from definition classes 808063A7/808084D7.

`live-52852-hold.bin` is the unmodified 128-byte timeline header from the PID 52852 capture at rescue-graph-52852-1788653632. `live-52852-selector.bin` is a replay image: the compiled runtime template supplies the uncaptured intermediate parent nodes; the actual captured 128-byte selector header and 41 node prefixes overwrite it, and node-table relative links are reconstructed from captured addresses. The production observer test replaces only the root self handle for its owned-memory resolver. It checks the exact parent chain, weak child salt, timeline header and entrance event. This is not a complete live process dump or proof of audible playback.
