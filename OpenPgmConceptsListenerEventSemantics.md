The arrival of an inbound message is an important event for most OpenPGM programs. To receive messages, programs create listener events, define callback functions to process the inbound messages, and dispatch events in a loop.

Each time the callback function runs, it receives an inbound message as an argument. The callback function must process the message in an appropriate application-specific fashion.

<img src='http://miru.hk/wiki/OpenPGM_listener_events.png' />

OpenPGM still processes incoming messages as the listener callback is being executed.  If the incoming rate is faster than the callback handler can handle the message queue will increase until the system is exhausted.

During error recovery it is possible for one incoming PGM packet to trigger the delivery of many messages to the application.  The default asynchronous queue mechanism executes one callback per message, which might be inefficient for some applications.  It is recommended for such high speed applications to read directly from the queue in a loop until either the queue is empty, or a predefined limit has been reached.  Reading until dry without sufficient cores to execute each PGM thread in parallel can cause the system to deadlock.
