/**
 *  Copyright (C) 2011, Jerzy Chalupski
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdio>
#include <cassert>
#include <vector>
#include <algorithm>
#include <list>
#include <set>
#include <string>
#include <fstream>
#include <stdexcept>
#include <array>

#include "polarssl/sha1.h"

using namespace std;

namespace {
    const char KWordListFileName[] = "Word-List.txt";
    const char KEncodedFileName[] = "Word-List.dat";

    const int KChildBitShift = 8;
    const int KChildIndexMask = 0x0FFFFF00;
    const int KLetterMask = 0x000000FF;
    const int KEndOfWordFlag = 0x20000000;
    const int KEndOfListFlag = 0x10000000;

    const size_t KHashSize = 20;
}

typedef array<unsigned char, KHashSize> Hash;

class GraphNode;
typedef set<GraphNode*, bool(*)(const GraphNode*, const GraphNode*)> UniqueNodeSet;

class GraphNode
{

public:
    GraphNode() :
        mEndOfWord(false),
        mValue(' '),
        mDepthGroup(-1),
        mIsDirectChild(true),
        mEndOfDawgList(false),
        mDawgIndex(-1)
    {
    }

public:
    vector<GraphNode*> mParents;
    vector<GraphNode*> mChildren;
    bool mEndOfWord;
    unsigned char mValue;
    int mDepthGroup;
    bool mIsDirectChild;

    bool mEndOfDawgList;
    int mDawgIndex;

    Hash mSha1;

    GraphNode(unsigned char childValue, int depthGroup, GraphNode *parent) :
        mEndOfWord(false),
        mValue(childValue),
        mDepthGroup(depthGroup),
        mIsDirectChild(false),
        mEndOfDawgList(false),
        mDawgIndex(-1)
    {
        mParents.push_back(parent);
    }

public:
    static bool compareByHashThenDirect(const GraphNode* one, const GraphNode* other) {
        int hashCompare = memcmp(one->mSha1.data(), other->mSha1.data(), KHashSize);
        if (hashCompare == 0) {
            if (one->mIsDirectChild == other->mIsDirectChild) {
                return one < other;
            } else {
                return one->mIsDirectChild < other->mIsDirectChild;
            }
        } else {
            return hashCompare < 0;
        }
    }

    ~GraphNode() {
        if (mIsDirectChild) {
            for (auto i = mChildren.begin(); i != mChildren.end(); ++i) {
                delete *i;
            }
        }
    }

    void calculateHash(const vector<unsigned char> &brothersHash = vector<unsigned char>()) {
        vector<unsigned char> hashInput;

        // We're iterating through children backwards, so the intermediate values of
        // hashInput are in fact brothersHash of successive children.
        for (auto i = mChildren.rbegin(); i != mChildren.rend(); ++i) {
            (*i)->calculateHash(hashInput);
            hashInput.insert(hashInput.end(), (*i)->mSha1.cbegin(), (*i)->mSha1.cend());
        }

        hashInput.push_back(mValue);
        hashInput.push_back(mEndOfWord);
        hashInput.insert(hashInput.end(), brothersHash.cbegin(), brothersHash.cend());

        sha1(&hashInput[0], hashInput.size(), mSha1.data());
    }

    void indexNodes(vector<GraphNode*> &indexedNodes) {
        if (!mChildren.empty() && mChildren.front()->mIsDirectChild && mChildren.front()->mDawgIndex == -1) {
            for (auto i = mChildren.begin(); i != mChildren.end(); ++i) {
                (*i)->mDawgIndex = indexedNodes.size() + 1;
                indexedNodes.push_back(*i);
            }

            for (auto i = mChildren.begin(); i != mChildren.end(); ++i) {
                (*i)->indexNodes(indexedNodes);
            }
        }
    }

    void markFirstAndLastChild() {
        if (!mChildren.empty()) {

            mChildren.front()->mIsDirectChild = true;
            mChildren.back()->mEndOfDawgList = true;

            for (auto i = mChildren.begin(); i != mChildren.end(); ++i) {
                (*i)->markFirstAndLastChild();
            }
        }
    }

    void findNodesAtDepth(const int depth, UniqueNodeSet &result) {
        for (auto i = mChildren.begin(); i != mChildren.end(); ++i) {
            if (depth <= (*i)->mDepthGroup) {
                if (depth == (*i)->mDepthGroup) {
                    result.insert(*i);
                }
                (*i)->findNodesAtDepth(depth, result);
            }
        }
    }

    bool haveSameHashAs(GraphNode *other) {
        assert(other != this);
        return memcmp(mSha1.data(), other->mSha1.data(), KHashSize) == 0;
    }

    vector<GraphNode*> getNextNodes() {
        assert(!mParents.empty());
        const vector<GraphNode*> &brothers = (*mParents.begin())->mChildren;
        return vector<GraphNode*>(find(brothers.begin(), brothers.end(), this), brothers.end());
    }

    void replaceWith(GraphNode* node, UniqueNodeSet &depthGroup) {
        auto oldChildren = getNextNodes();
        auto newChildren = node->getNextNodes();
        assert(oldChildren.size() == newChildren.size());

        auto newChild = newChildren.end() - 1;
        for (auto oldChild = oldChildren.rbegin(); oldChild != oldChildren.rend(); ++oldChild, --newChild) {
            for (auto parent = (*oldChild)->mParents.begin(); parent != (*oldChild)->mParents.end(); ++parent) {
                (*newChild)->mParents.push_back(*parent);

                if ((*parent)->mChildren.front() == *oldChild) {
                    (*parent)->mChildren = vector<GraphNode*>(newChild, newChildren.end());
                }
            }
        }

        while (oldChildren.size() > 1) {
            depthGroup.erase(oldChildren.back());
            oldChildren.pop_back();
        }
    }

    int encoded() {
        assert(mDawgIndex != -1);
        int result = mChildren.empty() ? 0 : mChildren.front()->mDawgIndex;
        assert(result != -1);
        result <<= KChildBitShift;
        result += mValue;
        if (mEndOfWord) result += KEndOfWordFlag;
        if (mEndOfDawgList) result += KEndOfListFlag;
        return result;
    }

    GraphNode* findChild(unsigned char childValue) {
        for (auto i = mChildren.begin(); i != mChildren.end(); ++i) {
            if ((*i)->mValue == childValue) {
                return (*i);
            }
        }
        return NULL;
    }

    GraphNode* addChild(unsigned char childValue, int depthGroup) {
        GraphNode* newChild = new GraphNode(childValue, depthGroup, this);
        mChildren.push_back(newChild);
        return newChild;
    }
};

vector<string> readWordList() {
    vector<string> output;
    string word;
    ifstream input(KWordListFileName);
    if (!input.is_open()) {
        throw ios_base::failure("Cannot open word list");
    }
    while (input.good()) {
        input >> word;
        if (input.good()) {
            output.push_back(word);
        }
    }
    input.close();
    return output;
}

bool sortByLengthThenAlphabetically(const string &one, const string &other) {
    if (one.length() == other.length()) {
        return one.compare(other) < 0;
    } else {
        return one.length() < other.length();
    }
}

template <class Iter>
Hash calculateWordListChecksum(Iter first, Iter last) {
    Hash result;
    if (last - first == 1) {
        string word = *first;
        sha1((unsigned char*)word.c_str(), word.length(), result.data());
    } else {
        array<unsigned char, KHashSize * 2> mergedHashes;

        Iter middle = first + (last - first) / 2;
        Hash leftHash = calculateWordListChecksum(first, middle);
        Hash rightHash = calculateWordListChecksum(middle, last);

        merge(leftHash.cbegin(), leftHash.cend(), rightHash.cbegin(), rightHash.cend(), mergedHashes.begin());

        sha1(mergedHashes.data(), mergedHashes.size(), result.data());
    }
    return result;
}

void buildTrie(const vector<string> &words, GraphNode &rootNode) {
    for (auto word = words.crbegin(); word != words.crend(); ++word) {
        int currentDepth = word->length() - 1;
        GraphNode* currentNode = &rootNode;

        for (auto letter = word->begin(); letter != word->end(); ++letter) {
            GraphNode* nextNode = currentNode->findChild(*letter);
            if (nextNode == NULL) {
                GraphNode* newNode = currentNode->addChild(*letter, currentDepth);
                currentNode = newNode;
            } else {
                currentNode = nextNode;
            }
            --currentDepth;
        }
        currentNode->mEndOfWord = true;
    }
}

void reduceGraph(GraphNode &rootNode, int maxNodeDepth) {
    for (int currentDepth = maxNodeDepth; currentDepth >= 0; --currentDepth) {
        printf("Depth %2d: ", currentDepth);

        UniqueNodeSet uniqueNodes(GraphNode::compareByHashThenDirect);
        rootNode.findNodesAtDepth(currentDepth, uniqueNodes);

        printf("%d nodes\n", uniqueNodes.size());

        if (uniqueNodes.size() > 1) {
            for (auto node = uniqueNodes.begin(), nextNode = node; node != uniqueNodes.end(); node = nextNode) {
                ++nextNode;

                // skip until nextNode points to a node that can be replaced by the node: a direct child with
                // identical hash as node.
                if ((*node)->mIsDirectChild == false) {
                    while (nextNode != uniqueNodes.end() && (*nextNode)->haveSameHashAs(*node) && (*nextNode)->mIsDirectChild == false) {
                        ++nextNode;
                    }
                }

                // there might be multiple nodes that can be replaced with node. Replace them all in loop and update nextNode to point
                // at the node after replaced node
                while (nextNode != uniqueNodes.end() && (*nextNode)->haveSameHashAs(*node)) {
                    (*nextNode)->replaceWith(*node, uniqueNodes);
                    uniqueNodes.erase(nextNode++);
                }
            }
        }
    }
}


void encodeGraph(const vector<GraphNode*> &indexedNodes) {
    ofstream output(KEncodedFileName, fstream::out | fstream::binary);
    if (!output.is_open()) {
        throw ios_base::failure("Cannot open binary file");
    }

    int numberOfNodes = indexedNodes.size() + 1;
    output.write(reinterpret_cast<char*>(&numberOfNodes), sizeof(int));

    int emptyZeroNode = 0;
    output.write(reinterpret_cast<char*>(&emptyZeroNode), sizeof(int));

    for (auto i = indexedNodes.begin(); i != indexedNodes.end(); ++i) {
        int encoded = (*i)->encoded();
        output.write(reinterpret_cast<char*>(&encoded), sizeof(int));
    }
    output.close();
}

void findWordsInBinaryNodes(int *nodes, int position, string prefix, vector<string>& output) {
    int node = nodes[position];

    char letter = node & KLetterMask;
    int childIndex = (node & KChildIndexMask) >> KChildBitShift;
    bool endOfList = (node & KEndOfListFlag) != 0;
    bool endOfWord = (node & KEndOfWordFlag) != 0;

    if (endOfWord) {
        output.push_back(string(prefix + letter));
    }

    if (!endOfList) {
        findWordsInBinaryNodes(nodes, position + 1, prefix, output);
    }

    if (childIndex != 0) {
        findWordsInBinaryNodes(nodes, childIndex, prefix + letter, output);
    }
}

void testEncodedGraph(const Hash &expectedChecksum) {
    ifstream input(KEncodedFileName, fstream::in | fstream::binary);
    if (!input.is_open()) {
        throw ios_base::failure("Cannot open binary file");
    }

    int nodeCount;
    input.read(reinterpret_cast<char*>(&nodeCount), sizeof(int));
    int *nodes = new int[nodeCount];
    for (int i(0); i != nodeCount; ++i) {
        input.read(reinterpret_cast<char*>(nodes + i), sizeof(int));
    }
    input.close();

    vector<string> wordList;
    findWordsInBinaryNodes(nodes, 1, "", wordList);

    sort(wordList.begin(), wordList.end(), sortByLengthThenAlphabetically);

    Hash binaryOutput = calculateWordListChecksum(wordList.cbegin(), wordList.cend());
    assert(equal(binaryOutput.cbegin(), binaryOutput.cend(), expectedChecksum.cbegin()));

    delete [] nodes;
}

int main(int argc, char* argv[]) {
    try {
        printf("Reading word list\n");
        vector<string> allWords = readWordList();

        sort(allWords.begin(), allWords.end(), sortByLengthThenAlphabetically);
        int maxWordLength = allWords.back().length();

        printf("Calculate input checksum\n");
        Hash inputChecksum = calculateWordListChecksum(allWords.cbegin(), allWords.cend());

        printf("Creating a trie\n");
        GraphNode rootNode;
        buildTrie(allWords, rootNode);

        rootNode.markFirstAndLastChild();

        printf("Calculating hash for all nodes\n");
        rootNode.calculateHash();

        printf("Removing redundant nodes\n");
        reduceGraph(rootNode, maxWordLength - 1);

        printf("Preparing final node list\n");
        vector<GraphNode*> indexedNodes;
        rootNode.indexNodes(indexedNodes);
        assert(indexedNodes.size() < (KChildIndexMask >> KChildBitShift));
        printf("Will save %d nodes\n", indexedNodes.size());

        printf("Encoding graph\n");
        encodeGraph(indexedNodes);

        printf("Testing procedure - recreate from binary file\n");
        testEncodedGraph(inputChecksum);
    } catch (exception &e) {
        printf("%s\n", e.what());
        return -1;
    }

    return 0;
}
