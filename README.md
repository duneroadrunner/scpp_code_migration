Apr 2026

### Code migration via naturally and artificially intelligent agents

Currently, scpptool's [auto-translation feature](https://github.com/duneroadrunner/SaferCPlusPlus-AutoTranslation2) does a one-for-one translation from C source to the scpptool-enforced safe subset, but the result is not performance-optimal idiomatic code. "Idiomatic" code in the scpptool-enforced safe subset of C++ can, in a sense, be thought of as just "modern" C++ code that's a little more extreme in its modernity. As such, the distance between modern C++ code and idiomatic "safe" code is rather modest, and executing the transition should generally not be particularly daunting. 

For large code bases it might be somewhat tedious though. But these days AI agents are increasingly becoming the go-to solution for such tedious tasks. Of course, any commentary on the state of AI these days is generally obsolete as soon as it's written. But the current (Apr 2026) state of AI agents is such that the instructions required to get them to do the code migration task also reasonably serve as an explanation for us "naturally intelligent" agents about the details of how the idiomatic safe code works in practice.

So this repo contains a [summary of our episode](https://github.com/duneroadrunner/scpp_code_migration/tree/main/examples/simple_html_parser/) of getting a local AI agent to convert a simplistic html parser to the scpptool-enforced safe subset.
