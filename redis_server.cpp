/**
 * @file redis_server.cpp
 * @brief A minimal Redis-compatible TCP server supporting PING and ECHO.
 *
 * This server listens on port 6379 and communicates using RESP (Redis
 * Serialization Protocol). Each incoming client connection is dispatched
 * to its own thread and can issue multiple commands over a single connection.
 *
 * Supported commands:
 *   - PING  → +PONG\r\n
 *   - ECHO  → $<len>\r\n<message>\r\n
 *   - other → -ERR unknown command\r\n
 *
 * @author  you
 * @date    2026
 */

#include "resp_deserializer.h"
#include "resp_serializer.h"

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>

using std::cout;
using std::cerr;
using std::endl;
using std::string;


// =============================================================================
// § 1. Client handler
// =============================================================================

/**
 * @brief Reads, parses, and responds to RESP commands from a single client.
 *
 * Runs on its own thread for each connected client. Owns @p client_fd for
 * its lifetime and guarantees it is closed before returning, whether the
 * exit is due to a clean disconnect, a recv error, or a send error.
 *
 * The function uses two nested loops:
 *   - Outer loop: one iteration per complete RESP command.
 *   - Inner loop: one iteration per recv() call, accumulating bytes until
 *     a complete RESP message (ending with \\r\\n) has been received.
 *
 * This two-level structure is required because TCP is a stream protocol with
 * no built-in message boundaries — a single command may arrive in multiple
 * recv() calls, and multiple commands may arrive in one.
 *
 * @param client_fd  File descriptor for the accepted client connection.
 *                   Ownership transfers to this function; do not use the
 *                   descriptor elsewhere after calling this.
 */
void handle_client(int client_fd) {

    cout << "Client connected!" << endl;

    // -------------------------------------------------------------------------
    // § 1a. Outer loop — one full RESP command per iteration
    // -------------------------------------------------------------------------

    while (true) {

        string accumulated;  ///< Grows until it holds one complete RESP message.
        char buffer[1024];   ///< Stack buffer for raw bytes from recv().

        // ---------------------------------------------------------------------
        // § 1b. Inner loop — accumulate bytes until \r\n terminator
        // ---------------------------------------------------------------------

        while (true) {

            /**
             * recv() blocks until data is available, then copies up to
             * sizeof(buffer)-1 bytes into buffer.
             *
             * Return values:
             *   > 0  normal data — continue accumulating
             *   = 0  client closed the connection gracefully
             *   < 0  OS or network error
             */
            int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received < 0) {
                cerr << "recv error" << endl;
                close(client_fd);
                return;
            }

            if (bytes_received == 0) {
                cout << "Client disconnected." << endl;
                close(client_fd);
                return;
            }

            // Construct from explicit length so null bytes do not truncate.
            accumulated += string(buffer, bytes_received);

            // Every valid RESP message ends with \r\n. The >= 2 guard
            // prevents underflow when fewer than 2 bytes have arrived.
            if (accumulated.size() >= 2 &&
                accumulated.substr(accumulated.size() - 2) == "\r\n") {
                break;
            }

        } // end inner recv loop

        cout << "received: " << accumulated << endl;

        // ---------------------------------------------------------------------
        // § 1c. Parse — convert raw RESP bytes into a plain command string
        // ---------------------------------------------------------------------

        /**
         * de_serial() returns a space-separated token string, e.g.
         * "*2\r\n$4\r\necho\r\n$5\r\nhello\r\n" → "echo hello "
         *
         * The trailing space is an artefact of the deserializer appending a
         * space after each token. We strip it before matching.
         */
        string command = de_serial(accumulated);

        while (!command.empty() && command.back() == ' ') {
            command.pop_back();
        }

        cout << "parsed command: " << command << endl;

        // ---------------------------------------------------------------------
        // § 1d. Dispatch — route the command to its response handler
        // ---------------------------------------------------------------------

        string response;

        /**
         * command.find() returns the index of the first match, or string::npos
         * if not found. Comparing against npos lets us match the command
         * anywhere in the string, which is necessary because redis-cli may
         * prepend its own argv tokens before the actual command name.
         */
        if (command.find("ping") != string::npos) {

            /// PING: respond with a RESP simple string "+PONG\r\n".
            response = serialize_simple_string("PONG");

        } else if (command.find("echo ") != string::npos) {

            /**
             * ECHO: extract everything after "echo " and return it as a
             * RESP bulk string. find() gives us the position of the 'e' in
             * "echo "; adding 5 skips past "echo " (4 chars + 1 space).
             */
            size_t echo_pos = command.find("echo ");
            string message  = command.substr(echo_pos + 5);
            response = serialize_bulk_string(message);

        } else {

            /// Unknown command: return a RESP error response.
            response = "-ERR unknown command\r\n";
        }

        cout << "sending: " << response << endl;

        // ---------------------------------------------------------------------
        // § 1e. Send loop — guarantee all bytes are delivered
        // ---------------------------------------------------------------------

        /**
         * send() may transmit fewer bytes than requested on a busy network.
         * We loop, advancing a cursor through the response buffer, until
         * every byte has been sent or an error occurs.
         *
         * @note total_sent is size_t (unsigned) to match response.size().
         *       Mixing signed and unsigned in comparisons can produce silent
         *       bugs due to implicit conversion.
         */
        size_t total_sent = 0;

        while (total_sent < response.size()) {

            int sent = send(client_fd,
                            response.c_str() + total_sent,
                            response.size()  - total_sent,
                            0);

            if (sent < 0) {
                cerr << "send error" << endl;
                close(client_fd);
                return;
            }

            total_sent += sent;
        }

        // Do not close — fall back to the outer loop and wait for the next command.

    } // end outer command loop

} // end handle_client


// =============================================================================
// § 2. main — server bootstrap and accept loop
// =============================================================================

/**
 * @brief Program entry point. Sets up the listening socket and dispatches
 *        each incoming connection to its own thread.
 *
 * Sequence:
 *   1. Create a TCP socket.
 *   2. Set SO_REUSEADDR so the port can be re-bound immediately on restart.
 *   3. Bind to INADDR_ANY:6379.
 *   4. Listen for incoming connections.
 *   5. Loop forever: accept a client, spawn a detached thread for it.
 *
 * @return 0 on clean exit (unreachable in normal operation),
 *         1 if socket setup fails.
 */
int main() {

    // -------------------------------------------------------------------------
    // § 2a. Create the TCP socket
    // -------------------------------------------------------------------------

    /**
     * socket() arguments:
     *   AF_INET      — IPv4 address family
     *   SOCK_STREAM  — TCP: ordered, reliable, connection-oriented delivery
     *   0            — let the OS pick the default protocol (TCP for SOCK_STREAM)
     *
     * Returns a file descriptor on success, -1 on failure.
     */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        cerr << "Failed to create socket" << endl;
        return 1;
    }

    // -------------------------------------------------------------------------
    // § 2b. Allow immediate port reuse after restart
    // -------------------------------------------------------------------------

    /**
     * Without SO_REUSEADDR the OS holds the port in TIME_WAIT for ~2 minutes
     * after the server exits, causing "Address already in use" on restart.
     * Setting this option allows immediate re-binding, which is essential
     * during development.
     */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // -------------------------------------------------------------------------
    // § 2c. Configure the bind address
    // -------------------------------------------------------------------------

    /**
     * sockaddr_in describes an IPv4 endpoint.
     * memset zeroes all fields including compiler-inserted padding bytes,
     * preventing stale stack garbage from reaching the OS.
     */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family      = AF_INET;      ///< Must match AF_INET above.
    server_addr.sin_addr.s_addr = INADDR_ANY;   ///< Listen on all interfaces.
    server_addr.sin_port        = htons(6379);  ///< htons converts to big-endian network byte order.

    // -------------------------------------------------------------------------
    // § 2d. Bind
    // -------------------------------------------------------------------------

    /**
     * bind() registers our socket with the OS at the specified address/port.
     * The cast to sockaddr* is required because bind() takes a generic socket
     * address; sockaddr_in is layout-compatible but a different type.
     * The :: prefix forces resolution to the global-scope bind(), avoiding
     * any accidental name collision with locally visible symbols.
     */
    if (::bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Failed to bind" << endl;
        return 1;
    }

    // -------------------------------------------------------------------------
    // § 2e. Listen
    // -------------------------------------------------------------------------

    /**
     * listen() transitions the socket into a passive listening state.
     * The backlog (5) is the maximum number of completed connections the OS
     * will queue while we are busy in accept(). Excess connections are refused.
     */
    if (listen(server_fd, 5) < 0) {
        cerr << "Failed to listen" << endl;
        return 1;
    }

    cout << "Redis server listening on port 6379..." << endl;

    // -------------------------------------------------------------------------
    // § 2f. Accept loop
    // -------------------------------------------------------------------------

    /**
     * Runs indefinitely until the process is killed (e.g. Ctrl-C / SIGINT).
     * Each accepted client is handed off to a detached thread so the loop
     * returns to accept() immediately, enabling concurrent client handling.
     *
     * @note Detached threads cannot be individually cancelled. A production
     *       server would track threads explicitly and implement graceful
     *       shutdown. For a development server detach() is appropriate.
     */
    while (true) {

        /**
         * accept() blocks until a client completes the TCP handshake, then
         * returns a new file descriptor for that specific connection.
         * server_fd is unaffected and continues listening.
         * nullptr arguments discard the client's address information.
         */
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            // Transient error — log and retry rather than crash.
            cerr << "Failed to accept" << endl;
            continue;
        }

        /**
         * std::thread(handle_client, client_fd) starts a new OS thread
         * calling handle_client(client_fd). client_fd is copied by value
         * into the thread — safe and cheap for an integer.
         *
         * .detach() severs the std::thread object from the OS thread,
         * allowing both to outlive each other independently.
         */
        std::thread(handle_client, client_fd).detach();

    } // end accept loop

    // Unreachable, but documents intent and aids future refactoring.
    close(server_fd);
    return 0;

} // end main