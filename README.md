# dodo
dodo is a terminal-based todo list manager and pretty-printer.

## Usage
At a high level, dodo will take a todo list, convert it (if necessary) to a human-readable intermediate format, display it to the screen with a few bells and whistles, and inter it in ~/.dodo/stash for quick reference later.  Roughly speaking, a todo list a newline ('\n') delimited file whose records are space-indented lines with optional leading bullets (such as + or -).  Lines are annotated with the date entered into dodo, as well as with any decorations specified by the line attributes (read the help with `dodo --help`).  A todo list may be specified:

 * By name, e.g., `dodo foo`.  dodo will search in the stash for the list.
 * By path, e.g., `dodo relative/path/to/foo`. dodo open the specified file and save it in the stash as `foo`.
 * Via stdin, e.g., `cat bar | ./dodo foo`. dodo will read from stdin and save the result in the stash as `foo`.

Largely speaking, the third form is captured by the second form, except in cases where commandline tools are used to process a todo list.  Personally, I often find that I need to refocus a todo list when it gets too long, so I might do something like `dodo -L working_list | grep 'project' | dodo new_list` to take *working_list* down to only the elements related to *project*.  I don't know, it works for me.

## FAQ
Q) Do you know about org mode?

A) Yes.


Q) This is cool, where can I find the JIRA integration?

A) Please stop, you are frightening me.


Q) Would you like to know if this is broken?

A) Yes, please.
