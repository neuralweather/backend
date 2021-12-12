## Weather station backend

See `server.c` for the route handlers with database queries etc.
In `lib/` lies code for the HTTP server library (`http.c` / `http.h`), which is also self-written.
There you will also find `list.h`, a generic and type-safe linked-list implementation (through a lot of evil macro magic). The linked list is used by the web server e.g. for the request headers (which vary in number, hence the need for a linked list).

The linked list is already available as a standalone library (with detailed documentation available at https://github.com/lennardwalter/list.h).

The HTTP server framework is currently only available in this source tree. Even in its early stages it is easier to use than any of the alternatives (in C), a release on github is planned very soon.

These libraries were written specifically for the weather station project and should be considered part of the project. However, since they were written "just for fun" and are far outside the specifications of the specifications document, no attention was paid to consistent commenting. Most of it is pretty self-explanatory, though.

### Usage

To compile the weather station backend, you need to have `meson` installed, as well as the following dependencies:

-   `json-c`
-   `sqlite3`

Run `meson [builddir]` in the root directory of the project, where `[builddir]` is the directory where you want to build the project.
To build the server, execute `meson compile -C [builddir]`.
Afterwards you can start the server with the following command: `./[builddir]/server [host] [port] [db file]`
