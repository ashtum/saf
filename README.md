## What's the saf?

**saf** is a single-header Asio based **scheduler aware future/promise** that does not block a whole thread if you want to wait for future. instead it cooperates with Asio's executor like other Asio based io_objects (e.g. asio::steady_timer).

### Quick usage

The latest version of the single header can be downloaded from [`include/saf.hpp`](include/saf.hpp).

**NOTE**
If you are using stand-alone version of Asio, you should define `SAF_ASIO_STANDALONE` before including `saf.hpp`.
```c++
#include <saf.hpp>

```

### API
