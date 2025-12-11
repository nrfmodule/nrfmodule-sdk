# nrfmodule-sdk
Public distribution of the nRFModule library. Contains headers and pre-compiled binaries for customer integration.

## Overview
This SDK provides the public interface for the nRFModule library. It allows applications to communicate with the nRFModule hardware using standard Nordic APIs.

## Architecture
*   **Headers**: `include/` contains the public API definitions.
*   **Binaries**: `lib/` contains pre-compiled static libraries (`.a`) for customers who do not have access to the source code.
*   **Build System**: `CMakeLists.txt` automatically detects if the full source (`nrfmodule-core`) is present in the workspace and switches between source-compilation and binary-linking modes.
