This example code draws two clickable buttons. Each causes a sound to play,
fed to either the left or right audio channel through separate (planar)
arrays.

Planar audio can feed both channels at the same time from different arrays,
as well, but this example only uses one channel at a time for clarity. A
NULL array will supply silence for that channel.
