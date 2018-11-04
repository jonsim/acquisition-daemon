# acquisition-daemon
Framework for a daemon which acquires a shared resource and manages access to that resource between many processes.

All source files except `client.c` make up the acquisition daemon (`acquired`).
A minimally functional client implementaion is provided (`client.c`) purely for
reference to illustrate how the acquisition daemon is expected to be used.


## License

All files are licensed under
[the MIT license](https://github.com/jonsim/acquisition-daemon/blob/master/LICENSE).

&copy; Copyright 2018 Jonathan Simmonds
