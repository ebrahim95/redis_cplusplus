// =============================================================================
// A Minimal Redis Server in C++
// =============================================================================
//
// This program is a partial implementation of a Redis-compatible TCP server.
// Redis is an in-memory data store; clients connect to it over TCP on port 6379
// and communicate using a text protocol called RESP (Redis Serialization
// Protocol). Our server understands two commands: PING and ECHO. Every other
// command receives an error response.
//
// The program is organized into two logical sections:
//
//   1. handle_client  — reads, parses, and responds to commands from one client
//   2. main           — creates the listening socket and dispatches each
//                       incoming connection to its own thread
//
// We follow the RESP wire format throughout. A RESP message is a string whose
// first byte declares the data type ('+' simple string, '-' error, ':' integer,
// '$' bulk string, '*' array), followed by the payload and terminated by \r\n.
// For example, redis-cli sends PING as: *1\r\n$4\r\nping\r\n
// =============================================================================


// -----------------------------------------------------------------------------
// § 1. Headers
// -----------------------------------------------------------------------------
//
// C++ programs are assembled from separately compiled translation units. The
// #include directive pastes the named file into this one before compilation
// begins, making the declarations inside visible here.

// Our own RESP codec — declared in these two headers, defined in the
// corresponding .cpp files that are linked in at build time.
#include "resp_deserializer.h"  // de_serial()  — RESP bytes  → plain command string
#include "resp_serializer.h"    // serialize_*() — plain data  → RESP bytes

// Standard I/O streams. 'cout' writes to stdout, 'cerr' writes to stderr.
// Separating them matters: stderr is unbuffered, so error messages appear
// immediately even if stdout is being redirected to a file.
#include <iostream>

// std::string — a heap-allocated, growable sequence of characters. Unlike a
// raw C char array, it knows its own length, handles memory automatically, and
// supports operators like += and methods like .find(), .substr(), .back().
#include <ostream>
#include <string>

// POSIX socket API. A "socket" is an OS abstraction for a network endpoint —
// think of it as a file descriptor that you read/write bytes through, except
// the bytes travel over the network instead of a disk.
#include <sys/socket.h>   // socket(), bind(), listen(), accept(), recv(), send()

// sockaddr_in — the structure that describes an IPv4 address + port pair.
// INADDR_ANY and htons() also live here.
#include <netinet/in.h>

// POSIX file utilities. close() releases an open file descriptor back to the OS.
#include <unistd.h>

// memset() — fills a block of memory with a single byte value. We use it to
// zero-initialise the address structure before filling in our fields, ensuring
// no stale garbage bytes confuse the OS.
#include <cstring>

// std::thread — C++11's portable threading primitive. Constructing one
// immediately starts a new OS thread running the given callable.
#include <thread>

// Pull the names we use most into the current scope so we can write 'cout'
// instead of 'std::cout' every time. We avoid 'using namespace std' wholesale
// because that dumps hundreds of names into the global namespace and risks
// silent name collisions.
using std::cout;
using std::cerr;
using std::endl;
using std::string;


// =============================================================================
// § 2. handle_client — the per-connection command loop
// =============================================================================
//
// Each connected client runs this function on its own thread. The function
// owns the client file descriptor for its lifetime and is responsible for
// closing it before returning, regardless of how the connection ends.
//
// The overall structure is two nested loops:
//
//   Outer loop: one iteration per RESP command (keeps the connection alive)
//     Inner loop: one iteration per recv() call (assembles a complete message)
//
// This two-level loop is necessary because TCP is a stream protocol — it
// delivers a continuous river of bytes with no built-in message boundaries.
// We must reconstruct message boundaries ourselves by watching for the
// terminal \r\n sequence that every RESP message ends with.

// 'void' means this function produces no return value.
// 'int client_fd' is a file descriptor — a small integer the OS uses as a
// handle to a specific open resource, in this case a TCP connection.
void handle_client(int client_fd) {

    cout << "Client connected!" << endl;

    // -------------------------------------------------------------------------
    // § 2a. Outer loop — one full RESP command per iteration
    // -------------------------------------------------------------------------
    //
    // 'while (true)' creates an infinite loop. The only exits are 'return'
    // statements deep inside (on recv error or client disconnect). This keeps
    // the TCP connection alive across many commands, which is standard Redis
    // behaviour — a client connects once and issues many commands before
    // disconnecting.

    while (true) {

        // 'string accumulated' starts empty each time we enter this outer loop.
        // It will grow byte-by-byte until we have one complete RESP message.
        // Declaring it inside the loop means it is destroyed and re-created on
        // every iteration — a fresh slate for each new command.
        string accumulated;

        // 'char buffer[1024]' is a fixed-size array on the stack. The OS will
        // write raw bytes into it when recv() is called. 1024 bytes is
        // deliberately small — any single recv() call may return fewer bytes
        // than the full message, which is why we loop and accumulate.
        // Stack memory is automatically reclaimed when the enclosing scope ends.
        char buffer[1024];


        // ---------------------------------------------------------------------
        // § 2b. Inner loop — accumulate bytes until a complete RESP message
        // ---------------------------------------------------------------------
        //
        // recv() is a blocking call: it suspends this thread until the OS has
        // at least one byte to give us, then copies up to sizeof(buffer)-1 bytes
        // into 'buffer' and returns how many bytes it actually copied.
        //
        // Three outcomes are possible:
        //   bytes_received > 0  — normal data arrived, keep accumulating
        //   bytes_received == 0 — the client closed the connection gracefully
        //   bytes_received < 0  — a network or OS error occurred

        while (true) {

            // recv() arguments:
            //   client_fd          — which connection to read from
            //   buffer             — where to put the bytes
            //   sizeof(buffer) - 1 — maximum bytes to read; the -1 reserves
            //                        one byte so we could null-terminate if
            //                        needed (we don't rely on that here, but
            //                        it is a defensive habit)
            //   0                  — flags; 0 means default blocking behaviour
            //
            // 'int' is a signed 32-bit integer on virtually all platforms.
            // It is the correct type here because recv() returns -1 on error,
            // which an unsigned type could not represent.
            int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

            // A negative return value means recv() failed. errno would tell us
            // why, but for simplicity we treat all errors as fatal for this
            // connection. We close the fd to release the OS resource and
            // 'return' to exit this thread's function entirely.
            if (bytes_received < 0) {
                cerr << "recv error" << endl;
                close(client_fd);
                return;
            }

            // Zero means the peer performed an orderly shutdown — it called
            // close() or shutdown() on its end. No more data will ever arrive.
            // We close our end and return, ending this thread cleanly.
            if (bytes_received == 0) {
                cout << "Client disconnected." << endl;
                close(client_fd);
                return;
            }

            // We have bytes_received > 0 bytes of real data in 'buffer'.
            //
            // string(buffer, bytes_received) constructs a std::string from
            // exactly bytes_received characters of 'buffer'. This is safer than
            // string(buffer) which would stop at the first '\0' byte — a
            // null byte inside RESP data would silently truncate the message.
            //
            // '+=' appends the newly received chunk to whatever we have so far.
            accumulated += string(buffer, bytes_received);

            // Every valid RESP message ends with \r\n. Once we see those two
            // bytes at the tail of 'accumulated', we have a complete message
            // and can stop reading. We break out of the inner loop only.
            //
            // accumulated.size() returns a size_t (unsigned). The >= 2 guard
            // prevents underflow if only 0 or 1 bytes have arrived so far.
            //
            // .substr(accumulated.size() - 2) extracts the last two characters.
            // Comparing with "\r\n" uses std::string's == operator which checks
            // length and content, unlike C's strcmp.
            if (accumulated.size() >= 2 &&
                accumulated.substr(accumulated.size() - 2) == "\r\n") {
                break;  // 'break' exits the nearest enclosing loop — inner only
            }

        } // end inner recv loop — 'accumulated' now holds one complete RESP message


        cout << "received: " << accumulated << endl;


        // ---------------------------------------------------------------------
        // § 2c. Parse the RESP message into a plain-text command
        // ---------------------------------------------------------------------
        //
        // de_serial() is our RESP deserializer. It converts the raw RESP bytes
        // into a space-separated string of tokens. For example:
        //
        //   "*2\r\n$4\r\necho\r\n$5\r\nhello\r\n"  →  "echo hello "
        //
        // Notice the trailing space — de_serial appends a space after each
        // token. We strip it here so that downstream comparisons work cleanly.

        string command = de_serial(accumulated);
        cout << command << "deserial" << endl;

        // command.empty() — returns true if the string has zero characters.
        // We check this first to avoid calling .back() on an empty string,
        // which would be undefined behaviour (reading past the end of memory).
        //
        // command.back() — returns a reference to the last character.
        //
        // command.pop_back() — removes the last character in O(1) time by
        // decrementing the string's internal length counter.
        //
        // The 'while' (not 'if') handles the case where de_serial appends
        // multiple trailing spaces, stripping all of them.
        while (!command.empty() && command.back() == ' ') {
            command.pop_back();
        }

        cout << "parsed command: " << command << endl;


        // ---------------------------------------------------------------------
        // § 2d. Dispatch — decide what response to send
        // ---------------------------------------------------------------------
        //
        // We declare 'response' here as an empty string and fill it in one of
        // three branches. Declaring it outside the if/else means it is in scope
        // for the send loop that follows.

        string response;

        // command.find("ping") searches left-to-right for the substring "ping"
        // and returns the index of the first match, or string::npos (a very
        // large number meaning "not found") if absent.
        //
        // == 0 means the match starts at index 0, i.e. the command *begins*
        // with "ping". We also check command == "ping" for an exact match.
        // Both conditions are equivalent here since we stripped trailing spaces,
        // but the explicit equality check makes the intent clearer.
       if (command.find("ping") != string::npos) {

            // serialize_simple_string("PONG") produces "+PONG\r\n" —
            // a RESP simple string response. Redis clients display this as PONG.
            response = serialize_simple_string("PONG");

        } else if (command.find("echo ") != string::npos) {

            // The ECHO command repeats whatever argument it is given.
            // After stripping trailing spaces, "echo hello" is exactly 9 chars:
            //   e(0) c(1) h(2) o(3) (4) h(5) e(6) l(7) l(8) o(9-wait that's 10)
            // "echo " is 5 characters (e,c,h,o,space), so substr(5) gives
            // everything from index 5 onwards — the message argument.
            //
            // std::string::substr(pos) returns a new string containing all
            // characters from position 'pos' to the end.
            size_t echo_pos = command.find("echo ");
            string message = command.substr(echo_pos + 5);

            // serialize_bulk_string wraps the message in RESP bulk string format:
            //   "$<length>\r\n<message>\r\n"
            // Bulk strings are how Redis returns arbitrary byte sequences.
            response = serialize_bulk_string(message);

        } else {

            // Any unrecognised command gets a RESP error response. RESP errors
            // start with '-'. The client will display this as an error message.
            response = "-ERR unknown command\r\n";
        }

        cout << "sending: " << response << endl;


        // ---------------------------------------------------------------------
        // § 2e. Send loop — deliver the complete response
        // ---------------------------------------------------------------------
        //
        // Just as recv() may return fewer bytes than requested, send() may
        // transmit fewer bytes than we asked it to — especially on a busy or
        // congested network. We loop until every byte has been sent.
        //
        // 'size_t' is an unsigned integer type guaranteed to be large enough
        // to hold the size of any object in memory. It is the correct type for
        // byte counts and indices into strings/arrays because sizes are never
        // negative. Using 'int' here would risk overflow for large responses.

        size_t total_sent = 0;

        // response.size() returns the number of bytes in the response string.
        // We continue until total_sent accounts for all of them.
        while (total_sent < response.size()) {

            // send() arguments:
            //   client_fd                       — which connection to write to
            //   response.c_str() + total_sent   — pointer to the next unsent byte
            //                                     .c_str() gives a raw const char*
            //                                     pointer to the string's data;
            //                                     adding total_sent advances it
            //                                     past the bytes already sent
            //   response.size() - total_sent    — number of bytes still to send
            //   0                               — flags; 0 = default behaviour
            //
            // Returns the number of bytes actually sent, or -1 on error.
            int sent = send(client_fd, response.c_str() + total_sent,
                            response.size() - total_sent, 0);

            if (sent < 0) {
                cerr << "send error" << endl;
                close(client_fd);
                return;
            }

            // Advance our cursor by however many bytes were just sent.
            // On the next iteration we pick up exactly where we left off.
            total_sent += sent;
        }

        // We deliberately do NOT close client_fd or break here.
        // Falling through to the top of the outer while(true) loop means we
        // immediately wait for the client's next command on the same connection.
        // This mirrors real Redis behaviour: one connection, many commands.

    } // end outer command loop

} // end handle_client


// =============================================================================
// § 3. main — socket setup and the accept loop
// =============================================================================
//
// main() is the program entry point. The OS calls it when the process starts.
// Its job here is purely infrastructure: create a listening socket, configure
// it, and hand each incoming connection to a new thread. It never touches
// RESP data directly.
//
// 'int' return type: main() returns an exit code to the OS. 0 means success;
// any non-zero value signals failure. Shell scripts and process managers
// inspect this value.

int main() {

    // -------------------------------------------------------------------------
    // § 3a. Create the server socket
    // -------------------------------------------------------------------------
    //
    // socket() asks the OS to allocate a new socket and returns a file
    // descriptor for it. Arguments:
    //
    //   AF_INET     — Address Family Internet: use IPv4 addresses
    //   SOCK_STREAM — Streaming socket: TCP, which gives ordered, reliable,
    //                 connection-oriented byte delivery. The alternative
    //                 SOCK_DGRAM would give UDP (unreliable, connectionless).
    //   0           — Protocol: 0 lets the OS choose the default for the given
    //                 type, which is TCP for SOCK_STREAM.
    //
    // File descriptors are small non-negative integers. The OS keeps a table
    // (the "file descriptor table") mapping these integers to open resources.
    // On failure, socket() returns -1.

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        cerr << "Failed to create socket" << endl;
        return 1;  // Non-zero exit code signals failure to the OS/shell
    }


    // -------------------------------------------------------------------------
    // § 3b. Set SO_REUSEADDR
    // -------------------------------------------------------------------------
    //
    // When a server process exits, the OS keeps the port in a TIME_WAIT state
    // for up to 2 minutes to handle any delayed packets still in flight.
    // During this time, trying to bind the same port again fails with
    // "Address already in use". SO_REUSEADDR tells the OS to allow re-binding
    // immediately, which is essential during development when you restart the
    // server frequently.
    //
    // setsockopt() arguments:
    //   server_fd       — the socket to configure
    //   SOL_SOCKET      — the "level": we are setting a socket-level option
    //   SO_REUSEADDR    — the specific option name
    //   &opt            — pointer to the option value (1 = enable)
    //   sizeof(opt)     — size of the option value in bytes
    //
    // 'int opt = 1' — the value 1 means "enable this option". The type must be
    // int because that is what the socket API expects for boolean options.

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    // -------------------------------------------------------------------------
    // § 3c. Describe the address to listen on
    // -------------------------------------------------------------------------
    //
    // sockaddr_in is a C struct (a plain aggregate of named fields with no
    // methods) that describes an IPv4 endpoint — an IP address and a port.
    // The OS needs this information to know which incoming packets to route
    // to our socket.
    //
    // 'struct sockaddr_in server_addr' — in C++ the 'struct' keyword is
    // optional before a struct name, but it is common in socket code because
    // this API originated in C.

    struct sockaddr_in server_addr;

    // memset fills every byte of server_addr with 0. This zeroes all fields,
    // including any padding bytes the compiler may have inserted for alignment.
    // Without this, those padding bytes could contain arbitrary garbage from
    // previous stack use, which might confuse the OS.
    //
    // memset() arguments:
    //   &server_addr       — pointer to the memory to fill
    //   0                  — the byte value to fill with
    //   sizeof(server_addr)— number of bytes to fill
    memset(&server_addr, 0, sizeof(server_addr));

    // sin_family tells the OS which address family this struct describes.
    // It must match the AF_INET we passed to socket() above.
    server_addr.sin_family = AF_INET;

    // INADDR_ANY is the special address 0.0.0.0. It means "listen on all
    // available network interfaces" — whether the machine has one network
    // card or ten, connections arriving on any of them will reach us.
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // htons() — "host to network short" — converts a 16-bit integer from the
    // host's byte order to network byte order (big-endian). Network protocols
    // standardise on big-endian; most modern CPUs (x86, ARM) are little-endian.
    // Without this conversion the port number would be byte-swapped and the
    // OS would listen on the wrong port.
    // 6379 is Redis's well-known default port.
    server_addr.sin_port = htons(6379);


    // -------------------------------------------------------------------------
    // § 3d. Bind the socket to the address
    // -------------------------------------------------------------------------
    //
    // bind() associates our socket with the address we just described. After
    // this call, the OS knows that TCP packets arriving at port 6379 on any
    // interface should be delivered to server_fd.
    //
    // The cast (struct sockaddr*) is required because bind() accepts a generic
    // socket address pointer. sockaddr_in is an IPv4-specific layout; sockaddr
    // is the generic base type. C++ requires an explicit cast between them
    // because they are different struct types, even though sockaddr_in is
    // designed to be layout-compatible with sockaddr.
    //
    // The '::' prefix on bind() calls the global-scope bind(), not any
    // accidentally in-scope local name with the same spelling.

    if (::bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Failed to bind" << endl;
        return 1;
    }


    // -------------------------------------------------------------------------
    // § 3e. Start listening for incoming connections
    // -------------------------------------------------------------------------
    //
    // listen() transitions server_fd from an unconnected socket into a
    // "listening" socket. From this point the OS will accept incoming TCP
    // handshakes on our behalf and queue the resulting connections.
    //
    // The second argument (5) is the backlog — the maximum number of completed
    // connections that can sit in the OS queue waiting for us to call accept().
    // If the queue fills up before we accept, new connections are refused.
    // 5 is a modest but adequate value for a development server.

    if (listen(server_fd, 5) < 0) {
        cerr << "Failed to listen" << endl;
        return 1;
    }

    cout << "Redis server listening on port 6379..." << endl;


    // -------------------------------------------------------------------------
    // § 3f. Accept loop — one iteration per incoming client
    // -------------------------------------------------------------------------
    //
    // This loop never exits normally — the server runs until killed by a signal
    // (Ctrl-C sends SIGINT) or the process is terminated. The close(server_fd)
    // line at the bottom is unreachable in practice but is good style: it
    // documents intent and would matter if this loop were ever refactored.

    while (true) {

        // accept() blocks until a client completes the TCP three-way handshake.
        // It then removes that connection from the OS queue and returns a brand
        // new file descriptor specifically for that client. server_fd remains
        // unchanged and keeps listening for further connections.
        //
        // The two nullptr arguments would normally be pointers to a sockaddr_in
        // and a socklen_t to receive the client's IP and port. We pass nullptr
        // because we do not need that information here.
        //
        // On failure, accept() returns -1.

        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            // A failed accept is not necessarily fatal for the server — it might
            // be a transient OS error or an interrupted system call. We log the
            // error and loop back to try accepting again, rather than crashing.
            cerr << "Failed to accept" << endl;
            continue;  // 'continue' skips the rest of the loop body and goes
                       // straight back to the while(true) condition check
        }


        // ---------------------------------------------------------------------
        // § 3g. Dispatch the client to its own thread
        // ---------------------------------------------------------------------
        //
        // std::thread(handle_client, client_fd) constructs a new thread object.
        // The constructor immediately starts a new OS thread that calls
        // handle_client(client_fd). 'client_fd' is copied by value into the
        // thread — it is just an integer, so copying is safe and cheap.
        //
        // .detach() severs the link between this std::thread object and the
        // underlying OS thread. After detach():
        //
        //   - The OS thread runs independently until handle_client returns
        //   - The std::thread object can be safely destroyed (which happens
        //     immediately here since it is a temporary with no name)
        //   - We no longer need to call .join() to wait for it
        //
        // The alternative to detach() would be .join(), which would block the
        // main thread until that client disconnected — meaning only one client
        // could be served at a time. detach() is correct here because we want
        // unlimited concurrency: each client gets its own independent thread
        // and the main loop immediately goes back to accepting new ones.
        //
        // One trade-off: detached threads cannot be individually cancelled or
        // monitored. For a production server you would maintain a list of
        // threads and implement graceful shutdown. For our purposes, detach
        // is the right call.

        std::thread(handle_client, client_fd).detach();

    } // end accept loop


    // Unreachable in normal operation, but correct to include.
    // close() releases server_fd back to the OS, freeing the port.
    close(server_fd);
    return 0;  // 0 = success
}