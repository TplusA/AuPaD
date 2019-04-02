# Audio Path daemon

## Copyright and contact

AuPaD is released under the terms of the GNU General Public License version 2
or (at your option) any later version. See file <tt>COPYING</tt> for licensing
terms of the GNU General Public License version 2, or <tt>COPYING.GPLv3</tt>
for licensing terms of the GNU General Public License version 3.

Contact:

    T+A elektroakustik GmbH & Co. KG
    Planckstrasse 11
    32052 Herford
    Germany

## Short description

_AuPaD_ maintains current information about audio paths in the appliance the
Streaming Board is built into. The appliance may update information about audio
signal routing and parameters of elements the audio signal passes according to
its internal system knowledge, and external programs may read out these
information for their purposes.

## Communication with _dcpd_

### Initialization

After startup, _AuPaD_ assumes nothing about the appliance or its signal paths.
No guesswork is done here.

Initial audio path information are queried by _AuPaD_ from the SPI slave by
sending an according D-Bus request to _dcpd_, which will forward the request to
the appliance via register 82. The appliance will (hopefully) react and send
all audio path information in its entirety using the mechanism described below.

### Updates from the appliance

The SPI slave sends its audio path information to _dcpd_ via register 82 using
a simple description language (_AuPaL_, Audio Path Language), and _dcpd_ turns
fragments of audio path changes into equivalent JSON objects. These JSON
objects are emitted as D-Bus signals. _AuPaD_ connects to _dcpd_ and waits for
any JSON objects to be processed.

### Change requests to the appliance

External programs may want to change audio paths or parameters (such as
equalizer settings). Requests for such changes can be sent directly to _AuPaD_
via D-Bus, which will sanitize the request and forward it to _dcpd_ for
processing by the SPI slave via register 82. _dcpd_ turns the requests into
compact _AuPaL_ specifications so that the SPI slave doesn't have to deal with
JSON.

Successful changes are reported back via the regular update mechanism.

## Communication with other system processes

### Filter plugins and monitor objects

There are D-Bus objects with predefined filtered views on the audio paths.
These objects are _monitor objects_ which are connected to _AuPaD_'s internal
filter plugins. External programs need to register themselves on a monitor
object in order to receive audio path change information. _AuPaD_ will send
filtered information about changes via D-Bus signals as long as there is at
least one client registered.

### Updates from the appliance

On any audio path changes, _AuPaD_ emits D-Bus signals on each monitor object
with at least one registered client. These signals contain JSON objects
describing the changes.

### Initialization

External programs may request full audio path information via D-Bus method call
on monitor objects. They will typically do this after successful registration
with the corresponding object. Since these information may not be available the
instant they are requested (especially during system initialization), clients
are strongly advised to make their D-Bus call asynchronous.

The D-Bus call will fail immediately in case there is no audio path information
available. In this case, the client shall _not_ repeat its request, but instead
wait for a D-Bus signal containing full information sent to all registered
clients.

Clients shall ignore any audio path updates received via D-Bus signals before
initialization. It is guaranteed that any change information sent before
returning full information will be incorporated in the full information
returned by the initialization method call.

### Change requests

External programs may send JSON objects containing partial or full audio path
specifications via D-Bus method call. The appliance should update its internal
settings accordingly. When done, the appliance reports its changes back to the
Streaming Board, and _AuPaD_ distributes the change information as described
above.

Change reports must be viewed in disconnect from change request. Programs that
send change requests must never assume any specific outcome of their requests;
change requests may be ignored, fully or partially, and for various reasons.
All that a client can do is hope for the best and react to changes reported
back from the system.
