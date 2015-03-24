# Chord
Chord Implementation for Distributed Information Systems

based on http://pdos.csail.mit.edu/papers/chord:sigcomm01/chord_sigcomm.pdf and http://userpages.umbc.edu/~rfink1/621/Chewbacca.pdf

Run `make`, then create chord ring and nodes. Kill nodes using CTRL-C. Run query commands.

Example:
  
Terminal 1  
`make  
./chord 5432`  
Terminal 2  
`./chord 5400 127.0.0.1 5432`  
Terminal 3  
`./chord 5300 127.0.0.1 5432`  
Terminal 4  
`./query 127.0.0.1 5432  
Harry Potter  
Gettysburg Address
The Art of Computer Programming
quit
./query 127.0.0.1 5432 fetch_pre
./query 127.0.0.1 5400 fetch_suc
./query 127.0.0.1 5300 print_table`
Terminal 2
CTRL-C
Terminal 3
CTRL-C
Terminal 4
`./query 127.0.0.1 5432 fetch_suc
./query 127.0.0.1 5432 fetch_pre'