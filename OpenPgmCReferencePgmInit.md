_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
int *pgm_init* (void);<br>
</pre>

### Purpose ###
Create and start OpenPGM internal machinery.

### Remarks ###
This call creates the internal machinery that OpenPGM requires for its operation:

  * Threading support.
  * Timer support.
  * PGM protocol number.

Until the first call to <tt>pgm_init()</tt>, all events, queues, and transports are unusable.

### Return Value ###
On success, returns 0.

### See Also ###
  * [Environment](OpenPgmCReferenceEnvironment.md) in OpenPGM C Reference.