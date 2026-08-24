# Tested v1.2 checksums

These hashes correspond to the v1.2 package that was tested during development before the repository was assembled.

```text
SHA256  dinput8.dll
4cedb9851b35e15940ad3c129147c5315372dcf38c66aaa50716a8d45acab1c8

SHA256  WDForceStereo_v1.2.zip
b4e64009a12259dc438e4fc6a6e14e81088711a8b618facaa08ba9e647bd3037
```

A CI-built DLL may not be byte-for-byte identical if the GitHub runner uses a different Clang/LLD version. Functional equivalence should be checked through the exported functions and runtime log rather than assuming reproducible binary output across toolchain versions.
