This example code loads a few bitmap files from disk using the asynchronous
i/o, and then draws it to the window. It uses a task group to watch multiple
reads and deal with them in whatever order they finish.

Note that for a single tiny file like this, you'd probably not want to bother
with async i/o in real life, but this is just an example of how to do it.
