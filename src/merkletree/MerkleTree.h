#ifndef MERKLETREE_H
#define MERKLETREE_H

#include <stddef.h>
#include <vector>

#include "../include/types.h"
#include "TreeHasher.h"

class SerialHasher;

// Class for manipulating Merkle Hash Trees, as specified in the
// Certificate Transparency specificationdoc/sunlight.xml
// Implement binary Merkle Hash Trees, using an arbitrary hash function
// provided by the SerialHasher interface.
// Rather than using the hash function directly, we use a TreeHasher that
// does domain separation for leaves and nodes, and thus ensures collision
// resistance.
//
// This class is thread-compatible, but not thread-safe.
class MerkleTree {
 public:
  // The constructor takes a pointer to some concrete hash function
  // instantiation of the SerialHasher abstract class.
  // Takes ownership of the hasher.
  explicit MerkleTree(SerialHasher *hasher);
  ~MerkleTree();

  // Length of a node (i.e., a hash), in bytes.
  size_t NodeSize() const { return treehasher_.DigestSize(); };

  // Number of leaves in the tree.
  size_t LeafCount() const { return tree_.empty() ? 0 : tree_[0].size(); }

  // The |leaf|th leaf hash in the tree. Indexing starts from 1.
  bstring LeafHash(size_t leaf) const {
    if (leaf == 0 || leaf > LeafCount())
      return bstring();
    return tree_[0][leaf-1];
  }

  // Return the leaf hash, but do not append the data to the tree.
  bstring LeafHash(const bstring &data) {
    return treehasher_.HashLeaf(data);
  }

  // Number of levels. An empty tree has 0 levels, a tree with 1 leaf has
  // 1 level, a tree with 2 leaves has 2 levels, and a tree with n leaves has
  // ceil(log2(n)) + 1 levels.
  size_t LevelCount() const { return level_count_; }

  // Add a new leaf to the hash tree. Stores the hash of the leaf data in the
  // tree structure, does not store the data itself.
  //
  // (We will evaluate the tree lazily, and not update the root here.)
  //
  // Returns the position of the leaf in the tree. Indexing starts at 1,
  // so position = number of leaves in the tree after this update.
  //
  // @param data Binary input blob
  size_t AddLeaf(const bstring &data);

  // Get the current root of the tree.
  // Update the root to reflect the current shape of the tree,
  // and return the tree digest.
  //
  // Returns the hash of an empty string if the tree has no leaves
  // (and hence, no root).
  bstring CurrentRoot();

  // Get the root of the tree for a previous snapshot,
  // where snapshot 0 is an empty tree, snapshot 1 is the tree with
  // 1 leaf, etc.
  //
  // Returns an empty string if the snapshot requested is in the future
  // (i.e., the tree is not large enough).
  //
  // @param snapshot point in time (= number of leaves at that point).
  bstring RootAtSnapshot(size_t snapshot);

  // Get the Merkle path from leaf to root.
  //
  // Returns a vector of node hashes, ordered by levels from leaf to root.
  // The first element is the sibling of the leaf hash, and the last element
  // is one below the root.
  // Returns an empty vector if the tree is not large enough
  // or the leaf index is 0.
  //
  // @param leaf the index of the leaf the path is for.
  std::vector<bstring> PathToCurrentRoot(size_t leaf);

  // Get the Merkle path from leaf to the root of a previous snapshot.
  //
  // Returns a vector of node hashes, ordered by levels from leaf to root.
  // The first element is the sibling of the leaf hash, and the last element
  // is one below the root.
  // Returns an empty vector if the leaf index is 0, the snapshot requested is in the
  // future or the snapshot tree is not large enough.
  //
  // @param leaf the index of the leaf the path is for.
  // @param snapshot point in time (= number of leaves at that point)
  std::vector<bstring> PathToRootAtSnapshot(size_t leaf, size_t snapshot);

  // Get the Merkle consistency proof between two snapshots.
  // Returns a vector of node hashes, ordered according to levels.
  // Returns an empty vector if snapshot1 is 0, snapshot 1 >= snapshot2,
  // or one of the snapshots requested is in the future.
  //
  // @param snapshot1 the first point in time
  // @param snapshot2 the second point in time
  std::vector<bstring> SnapshotConsistency(size_t snapshot1, size_t snapshot2);

 private:
  // Update to a given snapshot, return the root.
  bstring UpdateToSnapshot(size_t snapshot);
  // Return the root of a past snapshot.
  // If node is not NULL, additionally record the rightmost node
  // for the given snapshot and node_level.
  bstring RecomputePastSnapshot(size_t snapshot, size_t node_level,
                                bstring *node);
  // Path from a node at a given level (both indexed starting with 0)
  // to the root at a given snapshot.
  std::vector<bstring> PathFromNodeToRootAtSnapshot(size_t node_index,
                                                    size_t level,
                                                    size_t snapshot);
  // A container for nodes, organized according to levels and sorted
  // left-to-right in each level. tree_[0] is the leaf level, etc.
  // The hash of nodes tree_[i][j] and tree_[i][j+1] (j even) is stored
  // at tree_[i+1][j/2]. When tree_[i][j] is the last node of the level with
  // no right sibling, we store its dummy copy: tree_[i+1][j/2] = tree_[i][j].
  //
  // For example, a tree with 5 leaf hashes a0, a1, a2, a3, a4
  //
  //        __ hash__
  //       |         |
  //    __ h20__     a4
  //   |        |
  //  h10     h11
  //  | |     | |
  // a0 a1   a2 a3
  //
  // is internally represented, top-down
  //
  // --------
  // | hash |                        tree_[3]
  // --------------
  // | h20  | a4  |                  tree_[2]
  // -------------------
  // | h10  | h11 | a4 |             tree_[1]
  // -----------------------------
  // | a0   | a1  | a2 | a3 | a4 |   tree_[0]
  // -----------------------------
  //
  // Since the tree is append-only from the right, at any given point in time,
  // at each level, all nodes computed so far, except possibly the last node,
  // are fixed and will no longer change.
  std::vector< std::vector<bstring> > tree_;
  TreeHasher treehasher_;
  // Number of leaves propagated up to the root,
  // to keep track of lazy evaluation.
  size_t leaves_processed_;
  // The "true" level count for a fully evaluated tree.
  size_t level_count_;
};
#endif
