PurpleEgg
=======
PurpleEgg is an experiment in taking terminal-centric development to the next level. It's basic features are:
* Understanding of common project conventions for different languages and frameworks
* Creating temporary containers to install the dependencies for a project based on a configuration file (pegg.yaml)
* Creating a terminal user interface where projects are first class, and windows are tied to projects.

Command line usage
==================
The PurpleEgg command line client is `pegg`

`pegg create <template> <projectname>`: create a new project from the specified template. (Currently template is hardcoded to django!)

`pegg shell`: start a shell in the current directory, adjusting the path to pick up local binaries for Python virtual environments, local npm installs, and so forth. The shell is inside a docker container if pegg.yaml is found.

`pegg run`: Run an arbitrary command in the environment of `pegg shell`

License and Copyright
=====================
PurpleEgg is copyright Owen Taylor <otaylor@fishsoup.net>
and licensed under the terms of the GNU General Public License,
version 2 or later.
