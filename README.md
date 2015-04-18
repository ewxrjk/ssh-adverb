ssh-adverb
==========

ssh-adverb  is  almost  equivalent to ssh(1), but treats the command to
execute as a vector of strings in the manner of nice(1).

For example, the command:

    ssh remote echo '*'

will expand the asterisk on the server side and return a list of files.
In contrast:

    ssh-adverb remote echo '*'

will echo a single asterisk.


Installation
------------

If you got it from git:

    autoreconf -is

In any case:

    ./configure
    make check
    sudo make install

Documentation:

    man ssh-adverb

Author
------

Copyright © 2015 Richard Kettlewell

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
