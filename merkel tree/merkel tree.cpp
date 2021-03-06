// merkel tree.cpp : Defines the entry point for the console application.
//arranged by jonas jia 20180818

#include "stdafx.h"
#include <string>
#include <iostream> 
#include <assert.h>
#include <openssl/md5.h>
using namespace std;
#include <list>
const size_t MD5_DIGEST_LENGTH = 512;
char *hash_str_func(const std::string& value) {
	unsigned char* buf;
	size_t len = basic_hash(value.c_str(), value.length(), &buf);
	assert(len == MD5_DIGEST_LENGTH);
	return (char *)buf;
}
size_t basic_hash(const char* value, size_t len, unsigned char** hash_value) {
	*hash_value = new unsigned char[MD5_DIGEST_LENGTH];
	MD5((unsigned char *)value, len, *hash_value);

	return MD5_DIGEST_LENGTH;
}
// Recursive implementation of the build algorithm.
template<typename NodeType>
const NodeType *build_(NodeType *nodes[], size_t len) {

	if (len == 1) return new NodeType(nodes[0], nullptr);
	if (len == 2) return new NodeType(nodes[0], nodes[1]);

	size_t half = len % 2 == 0 ? len / 2 : len / 2 + 1;
	return new NodeType(build_(nodes, half), build_(nodes + half, len - half));
}

template<typename T, typename NodeType>
const NodeType *build(const std::list<T> &values) {

	NodeType *leaves[values.size()];
	int i = 0;
	for (auto value : values) {
		leaves[i++] = new NodeType(value);
	}

	return build_(leaves, values.size());
};

// methods for simplicity's sake.
template <typename T, char* (hash_func)(const T&), size_t hash_len>
class MerkleNode {
protected:

	std::unique_ptr<const MerkleNode> left_, right_;
	const char *hash_;
	const std::shared_ptr value_;

	/**
	* Computes the hash of the children nodes' respective hashes.
	* In other words, given a node N, with children (N1, N2), whose hash values are,
	* respectively, H1 and H2, computes:
	*
	*     H = hash(H1 || H2)
	*
	* where `||` represents the concatenation operation.
	* If the `right_` descendant is missing, it will simply duplicate the `left_` node's hash.
	*
	* For a "leaf" node (both descendants missing), it will use the `hash_func()` to compute the
	* hash of the stored `value_`.
	*/
	virtual const char *computeHash() const = 0;

public:

	/**
	* Builds a "leaf" node, with no descendants and a `value` that will be copied and stored.
	* We also compute the hash (`hash_func()`) of the value and store it in this node.
	*
	* We assume ownership of the pointer returned by the `hash_func()` which we assume has been
	* freshly allocated, and will be released upon destruction of this node.
	*/
	MerkleNode(const T &value) : value_(new T(value)), left_(nullptr), right_(nullptr) {
		hash_ = hash_func(value);
	}

	/**
	* Creates an intermediate node, storing the descendants as well as computing the compound hash.
	*/
	MerkleNode(const MerkleNode *left,
		const MerkleNode *right) :
		left_(left), right_(right), value_(nullptr) {
	}

	/**
	* Deletes the memory pointed to by the `hash_` pointer: if your `hash_func()` and/or the
	* `computeHash()` method implementation do not allocate this memory (or you do not wish to
	* free the allocated memory) remember to override this destructor's behavior too.
	*/
	virtual ~MerkleNode() {
		if (hash_) delete[](hash_);
	}

	/**
	* Recursively validate the subtree rooted in this node: if executed at the root of the tree,
	* it will validate the entire tree.
	*/
	virtual bool validate() const;

	/**
	* The length of the hash, also part of the template instantiation (`hash_len`).
	*
	* @see hash_len
	*/
	size_t len() const { return hash_len; }

	/**
	* Returns the buffer containing the hash value, of `len()` bytes length.
	*
	* @see len()
	*/
	const char *hash() const { return hash_; }

	bool hasChildren() const {
		return left_ || right_;
	}

	const MerkleNode * left() const { return left_.get(); }
	const MerkleNode * right() const { return right_.get(); }


	
};
template <typename T, char* (hash_func)(const T&), size_t hash_len>
bool MerkleNode<T, hash_func, hash_len>::validate() const{
	// If either child is not valid, the entire subtree is invalid too.
	if (left_ && !left_->validate()) {
		return false;
	}
	if (right_ && !right_->validate()) {
		return false;
	}

	std::unique_ptr<const char> computedHash(hasChildren() ? computeHash() : hash_func(*value_));
	return memcmp(hash_, computedHash.get(), len()) == 0;
}

class MD5StringMerkleNode : public MerkleNode<std::string, hash_str_func, MD5_DIGEST_LENGTH> {};


int main(int argc, char *argv[]) {

	list<string> nodes;
	const MD5StringMerkleNode* root;
  
	for (int i = 0; i < 33; ++i) {
		nodes.push_back("node #" + std::to_string(i));
	}

	root = build<std::string, MD5StringMerkleNode>(nodes);

	bool valid = root->validate();
    cout << "The tree is " << (valid ? "" : "not ") << "valid" << endl;
	delete(root);

	return valid ? EXIT_SUCCESS : EXIT_FAILURE;

}
/*
TEST(MerkleNodeTests, CannotForgeTree) {
	std::string sl[]{ "spring", "winter", "summer", "fall" };
	MD5StringMerkleNode *leaves[4];

	for (int i = 0; i < 4; ++i) {
		leaves[i] = new MD5StringMerkleNode(sl[i]);
	}

	MD5StringMerkleNode *a = new MD5StringMerkleNode(leaves[0], leaves[1]);
	MD5StringMerkleNode *b = new MD5StringMerkleNode(leaves[2], leaves[3]);
	MD5StringMerkleNode root(a, b);

	EXPECT_TRUE(root.hasChildren());
	EXPECT_FALSE(b->left() == nullptr);
	EXPECT_TRUE(root.validate());

	MD5StringMerkleNode *fake = new MD5StringMerkleNode("autumn");
	b->setNode(fake, MD5StringMerkleNode::RIGHT);
	EXPECT_FALSE(root.validate());
}

TEST(MerkleNodeTests, hasChildren) {
	MD5StringMerkleNode *a = new MD5StringMerkleNode("test");
	EXPECT_FALSE(a->hasChildren());

	MD5StringMerkleNode *b = new MD5StringMerkleNode("another node");
	MD5StringMerkleNode parent(a, b);
	EXPECT_TRUE(parent.hasChildren());
}

TEST(MerkleNodeTests, Equals) {
	MD5StringMerkleNode a("test");
	MD5StringMerkleNode b("test");

	// First, must be equal to itself.
	EXPECT_EQ(a, a);

	// Then equal to an identically constructed other.
	EXPECT_EQ(a, b);

	// Then, non-equal to a different one.
	MD5StringMerkleNode c("not a test");
	EXPECT_NE(a, c);
}


TEST(MerkleNodeTests, Build) {
	std::string sl[]{ "one", "two", "three", "four" };

	std::list<std::string> nodes;
	for (int i = 0; i < 4; ++i) {
		nodes.push_back(sl[i]);
	}

	const MD5StringMerkleNode *root = build<std::string, MD5StringMerkleNode>(nodes);
	ASSERT_NE(nullptr, root);
	EXPECT_TRUE(root->validate());
	delete (root);
}
*/