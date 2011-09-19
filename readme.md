# DAWG Generator

## What is DAWG?
DAWG is an acronym for Directed Acyclic Word Graph, a data structure for efficient storing of a set of strings which allows fast querying if the word set contains a given word. It's basically the tree with shared common prefixes and suffixes. For example a pair of words: "TOPS" and "TAPS" can be represented as a following DAWG:

      O
     / \
    T   P-[S]
     \ /
      A

(square brackets around node 'S' indicate the end of the word in this node; if we would add a word "TO" to our list of words, the square brackets would appear also in 'O' node)

## Reducing the size of graph
JohnPaul Adamovsky (http://www.pathcom.com/~vadco/dawg.html) noticed that the fully reduced prefix+suffix tree contains minimal number of nodes, but every node contains the list of child nodes which requires a lot of space: the bare minimum is the number of children and one node index per child. In case of DAWG representing a large set of words there might be thousands of nodes, and the index size becomes a major contributor to the size of a single node. Minimizing the number of nodes without regards for a node size is not optimal.

If list of children in every node is indexed with consecutive indices it's possible to store in each node only the index of the first child and End-Of-Children-List flag. Our TAPS/TOPS would be encoded as:

Node 1: 'T', first child index 2, end of children list
Node 2: 'A', first child index 4
Node 3: 'O', first child index 4, end of children list
Node 4: 'P', first child index 5, end of children list
Node 5: 'S', first child index 0 (i.e. no children), end of children list, end of word

Node 2 doesn't have the End-Of-Children-List flag set, which means there is an edge from parent to the next node (i.e. node 3).

Using this approach introduces some redundancy to the node graph. Consider a node X with leaf children A, B, C and node Y with leaf children D, B, C. It's not possible to share B and C leaves, because index of A and index of D should both be one less than index of B and those indices cannot be equal, because A and D are completely different nodes.

## Implementation
I have based my implementation on JohnPaul Adamovsky's work. I use the same structure for the final graph encoding, but intermediate structures and graph reduction algorithms are a bit different.

First step is creation of trie (i.e. tree with shared prefixes). Besides the obvious information, like list of parents, children, letter and end-of-word flag, every node contains the information about maximum depth of it's child nodes. At this point adding this info is trivial and allows optimization in the graph reduction step. When all words are added to the trie, the first and last child in every node are marked. We won't reorder children lists, so this marking can be safely done now. First child flag is used during graph reduction step, and last child mark is just End-Of-Children-List flag needed for final graph encoding.

During the most computationally expensive step - graph reduction - we'll compare a whole bunch of nodes to each other. Nodes can be marked as equal if and only if all children are equal, the brothers further on parent's children list are equal and of course the node letter and End-Of-Word flags match. Considering the depth of the tree the naive comparisons (i.e. iterating through all children/brothers) can be very expensive, so to speed up the algorithm before the graph reduction the hash is calculated for every node.

Not all graph nodes can be replaced, even if there is another node with identical node value, flags, children and brothers. Let's go back to the X[A, B, C], Y[D, B, C] example: both 'B' nodes are identical (same values and flags, no children, and identical 'C' nodes further in brothers list), but as I have shown before both copies are necessary. So replacing non-first child with another non-first child cannot be done. Replacing non-first child with first child is problematic too, because the replaced node was not a first child of it's parent, but the replacing node has the first child flag set, which can lead to replacing non-first child with another non-first child. Replacing first child with either type of node is fine.

So the graph reduction is basically finding the nodes with identical hash and replacing nodes with first child flag set with other nodes with the same hash. This can be done by sorting the node list by hash and first child flag and then iterating through the list once doing replacements on the go. We can speed up this process further by grouping nodes by maximum depth of child nodes, an information we added to the nodes at the trie creation stage. The node groups are iterated in descending order, because there are less nodes with high maximum child depth parameter and removing one node with high max child depth means removing the whole subtree from further computations.

When all redundant nodes are pruned, the remaining nodes are numbered, preserving the correct order of indices in child groups. The nodes are stored as a single 32-bit integer. 8 bits are used for a letter value, 2 bits are used for End-Of-Word and End-Of-Children-List flags, the remaining 22 bits are used to store the index of the first child. This format limits the size of the graph (only 2^22-1 = about 4M nodes can be stored), but it's enough for my needs. For example English Scrabble TWL06 requires only 120k nodes and similar dictionary for Polish language occupies only 350k nodes.

## Results
TWL06 with 178691 words is encoded as 120223 nodes, encoding takes about 4 seconds.
Polish Scrabble dictionary from http://sjp.pl/slownik/growy/ with 2753263 words is encoded as 359558 nodes, encoding takes about 55 seconds.

JohnPaul Adamovsky's program encodes TWL06 as 121173 nodes in 3 minutes 17 seconds; the program didn't finish encoding the Polish dictionary in few hours, so either there is a bug or the algortihm complexity is too high for that size of input (BTW: that's the sole reason for starting this project; I would have used his program if it had worked).

## Possible improvements
### Truly optimal graph
My solution produces 900 nodes less than the program I base my implementation on. The effective algorithm is almost the same, except for one small difference: during trie creation stage I don't sort children alphabetically. How does this detail affect the output? Take a look at node X with children A, B, C and node Y with children A and C. Without changing the order the X and Y nodes can't share any child nodes, but after swapping nodes A and B the original Y children could be pruned. I've written a naive algorithm, but I wasn't able to reduce it's complexity to sane level (encoding TWL06 took about 90 minutes) and the result was only 4% smaller, so I decided not to use it. 

For morbidly curious it goes something like this: create the trie, calculate hashes of all nodes (just like in current version) and then prepare the list of non-leaf nodes and sort it by the descending number of children. Prepare the list of stacks of nodes. For every node on the non-leaf list iterate through the stacks list and check if there is a stack with the same set or superset of the given node children (i.e. for node with children list A, B, C try to find a stack with top node containing A, B, C and possibly other nodes in any order). If such stack is found, push the node on it; if not, push the node on the new stack and append it to the stack list. After all nodes have been added, the bottom of the stacks should contain the minimum number of nodes required for DAWG encoding. Correct order of children in those child groups can be determined by iterating from the top of the stack to the bottom and reordering the children of lower nodes in such way that the children of upper nodes create the tail of children list (i.e. if there is a stack with lists [A, B, C, D], [D, A, B] and [B, D], reorder [D, A, B] to [A, B, D] - notice that reordered list ends with [B, D], which is a list on the higher level of the stack - and then reorder [A, B, C, D] to [C, A, B, D]. The final stack contains nodes with child lists [C, A, B, D], [A, B, D] and [B, D]). The child lists from the nodes above the bottom of the stack can be replaced with sub-lists of the first node's children.

### Bit packing
Using 32 bits per node is actually quite wasteful. For example for TWL06 DAWG only 24 bits could be used per node: 2 for flags, 5 for letters (for 26 unique values) and 17 for child index (120223 nodes). For that particular example it would reduce the size of encoded DAWG by 25%. For Polish dictionary it would be 15% reduction. The downside is a bit more complex encoding and decoding code.

### Binary file header
Bit packing described above would require some kind of header describing the size of encoded letter and index and the lookup table for letter decoding.

### Correct memory management
Currently the graph nodes are leaked. It's lame and I should fix it (actually I should have wrote the code correctly in a first place... woulda, shoulda, coulda), but I'm too lazy to do it right now. Feel free to send me an email that I should be ashamed of myself.

### Command line arguments
The names of expected input file and output file are currently hardcoded, and it would be nice to have them configurable through command line parameters.

## 3rd party libraries
I used code from polarssl (http://polarssl.org/source_code) for SHA-1 hash calculations.

## License
GPLv3
