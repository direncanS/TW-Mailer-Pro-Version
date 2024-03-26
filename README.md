# TW-Mailer – Pro Version

Extend the basic TW-Mailer project with some professional features.

## Features and Enhancements

- **Concurrent Server:** Refactor the server to operate concurrently instead of iteratively.
  - Utilize `fork()` or threads.
  - Identify and safeguard critical sections to avoid synchronization problems.

- **Authentication:**
  - Introduce a `LOGIN` command. Restrict access to other commands to authenticated users only (except `QUIT`).
  - Authentication should be performed using the internal LDAP server.
  - Limit login attempts to 3 per user and IP. Upon failure, blacklist the IP for 1 minute.
  - Persist the blacklist information.
  - Remove manual sender option in `SEND`. The sender should be set automatically based on session information post-login.
  - Automatically set the username in `LIST/READ/DEL` commands based on session information.

- **LDAP Integration:**
  - Use OpenLDAP C-API (Ubuntu Packet `libldap2-dev`, include file `<ldap.h>`, gcc Option `-lldap -llber`).
  - LDAP Server details: Host: `ldap.technikum-wien.at`, Port: `389`, Search Base: `dc=technikum-wien,dc=at`.

## Protocol Specification (Update)

### LOGIN

LOGIN\n
<LDAP username>\n
<password in plain text>\n

- Server response: `OK\n` (session enabled for all commands) or `ERR\n`.

### SEND

SEND\n
<Receiver>\n
<Subject (max. 80 chars)>\n
<message (multi-line; no length restrictions)>\n
.\n


- Command ends with a final dot.
- Server response: `OK\n` or `ERR\n`.

### LIST

LIST\n


- Server response includes the count of messages and subjects for the current user.

### READ

READ\n
<Message-Number>\n


- Server response: `OK\n` with the complete message content or `ERR\n`.

### DEL

DEL\n
<Message-Number>\n


- Server response: `OK\n` or `ERR\n`.

## Deliverables

- Commented client and server code.
- Makefile for targets “all” and “clean”.
- Executables.
  - Client and server architecture.
  - Used technologies and libraries.
  - Development strategy and protocol adaptations.
  - Synchronization methods.
  - Handling of large messages.




