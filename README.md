Sequence data types (think of python lists) that support efficient partition and
concatenation, aimed for use in parallel programming.

There are three alternative implementations:

 - arrays (baseline)
 - ropes
 - skip list arrays (based on skip lists)

A parallel version of a run-length encoding algorithm is included as a
motivating example.

For more information, have a look at the [technical
report](http://kkourt.io/papers/pards-tr-2015.pdf).
