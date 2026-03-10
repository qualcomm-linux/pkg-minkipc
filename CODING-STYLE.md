# Coding Style Guide

This document describes the code formatting standards for the **MinkIPC** project.

This project primarily follows the [Linux kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html), achieved by running
the [clang-format](https://clang.llvm.org/docs/ClangFormat.html) tool with the
`.clang-format` file provided at the top of the repository.
Exceptions to this style are made when calling APIs from other libraries which might follow a different style and when implementing an interface specified by a standard, e.g. GlobalPlatform.

## Code Formatting Guidelines


### Headers and Includes

- The body of each header file must be surrounded by an include guard (aka "header guard"). Their names shall be all caps, with words separated by the underscore character "_". For example, *__MINKCOM_H_*
- Header files should only `#include` what is necessary to allow a file that includes it to compile.
- Ordering header files as follows:
    - Standard headers
    - C system headers
    - Other libraries' headers
    - Related and local project's headers.
- Headers in each block must listed in alphabetical order.
- Separate each group with a blank line.

```
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#include "qcbor.h"

#include "MinkCom.h"
#include "object.h"
  ...
```

## Commenting Styles

- **General Documentation:**
Use [Doxygen](https://www.doxygen.nl/index.html) format.

Use of **block comments** is allowed for both short and detailed explanations of code.

  ```c
  ...
  
  /* Calculate the cube of z */
  if (z > 0) {
    return z * z * z;
  }
  ...
  ```

  ```c
  ...
  int arrayRaw[4] = { 1, 2, 3, 5 };

  /* This piece of code is primarily used for marshalling the
   * the contents of the array in a byte blob suitable for
   * transmission across a a transport channel.
   */
  for ( int i = 0; i < 4; i++ ) {
     marshalIntoPacket(arrayRaw[i]);
  }
  ...
  ```
