# dodo
dodo is a terminal-based todo list manager and pretty-printer.

## Usage
At a high level, dodo will take a todo list, convert it (if necessary) to its specific human-readable format, display it to the screen with a few bells and whistles, and stash it for reference later.  Roughly speaking, a todo list is a newline ('\n') delimited file whose records are space-indented lines with optional leading bullets (such as + or -).  Lines are annotated with the date entered into dodo, as well as with any decorations specified by the line attributes (read the help with `dodo --help`).  A todo list may be specified:

 * By name, e.g., `dodo foo`.  dodo will search in the stash for the list.
 * By path, e.g., `dodo relative/path/to/foo`. dodo will open the specified file and save it in the stash as `foo`.
 * Via stdin, e.g., `cat bar | dodo foo`. dodo will read from stdin and save the result in the stash as `foo`.

Largely, the third form is captured by the second form, except in cases where commandline tools are used to process a todo list.  Personally, I often find that I need to refocus a todo list when it gets too long, so I might do something like `dodo -L working_list | grep 'project' | dodo new_list` to take *working_list* down to only the elements related to *project*.  I don't know, it works for me.

### Format
A todo list is a newline-delimited text file whose lines begin with indented bullets.  It'll try to reason about bullet indentation in order to specify sub-items.

Once dodo has stashed a text file, each bullet is decorated with a line header enclosed in curly braces.  This header consists of a comma-separated tuple designating the datetime (YYMMDDhhmm) and additional attributes, such as coloration or special display modes.  Currently, dates are only populated at time of first entry--to change a date, one must do so manually.


#### Coloration
lowercase designates foreground color, uppercase is background color
* k,K - black
* r,R - red
* g,G - green
* y,Y - yellow
* b,B - blue
* m,M - magenta
* c,C - cyan
* w,W - white

#### Special Display Modes
These specify special VT-100 formatting modes
* 1 - bright (bold)
* 2 - dim
* 4 - underscore
* 5 - blink
* 7 - reverse

#### Special Status
Lines are either todo list entries or notes
* '$' - Done (todo)
* '!' - High Priority (todo)
* '#' - Star (todo)
* '@' - Heart (todo)
* '?' - Note

## Arguments
### Specifying a list
A list can be specified in one of two ways.  Either by providing an easy-to-remember shorthand to dodo, such as with `dodo -e groceries`, or by providing an absolute path, such as `dodo /foo/bar/baz`.

### Output
The --output (-o): output file should be saved in a location other than the stash

### Help
The --help (-h): display a short help screen

### Version
--version (-v): print the version and exit

### List
--list (-l): summarize the stashed todo lists

### Quiet
--quiet (-q): don't print to stdout

### Sort
--sort (-s): interactively questionnaire to help user prioritize tasks

### Dry Run
--dry_run (-d): don't write anything to file

### Edit
--edit (-e): open the specified todo list in the tool specified by $DODO_EDITOR (or, if undefined, $EDITOR)

### Display modes
* --noextra (-E)  Suppress cute visuals, like the heart or star
* --nonote  (-N)  Hide notes
* --nodone  (-D)  Hide completed entries
* --nokids  (-K)  Hide all children
* --noattr  (-A)  Do not print with colors or other attributes
* --literal (-L)  Print the results to stdout, using dodo's native format

## FAQ
Q) Do you know about org mode?

A) Yes.


Q) This is cool, where can I find the JIRA integration?

A) Please stop, you are frightening me.


Q) Would you like to know if this is broken?

A) Yes, please.
