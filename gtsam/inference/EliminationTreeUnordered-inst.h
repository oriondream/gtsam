/* ----------------------------------------------------------------------------

* GTSAM Copyright 2010, Georgia Tech Research Corporation,
* Atlanta, Georgia 30332-0415
* All Rights Reserved
* Authors: Frank Dellaert, et al. (see THANKS for the full author list)

* See LICENSE for the license information

* -------------------------------------------------------------------------- */

/**
* @file    EliminationTree-inl.h
* @author  Frank Dellaert
* @author  Richard Roberts
* @date    Oct 13, 2010
*/
#pragma once

#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <stack>

#include <gtsam/base/timing.h>
#include <gtsam/base/treeTraversal-inst.h>
#include <gtsam/inference/EliminationTreeUnordered.h>
#include <gtsam/inference/VariableIndexUnordered.h>
#include <gtsam/inference/OrderingUnordered.h>
#include <gtsam/inference/inference-inst.h>

namespace gtsam {

  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  typename EliminationTreeUnordered<BAYESNET,GRAPH>::sharedFactor
    EliminationTreeUnordered<BAYESNET,GRAPH>::Node::eliminate(
    const boost::shared_ptr<BayesNetType>& output,
    const Eliminate& function, const std::vector<sharedFactor>& childrenResults) const
  {
    // This function eliminates one node (Node::eliminate) - see below eliminate for the whole tree.

    assert(childrenResults.size() == children.size());

    // Gather factors
    std::vector<sharedFactor> gatheredFactors;
    gatheredFactors.reserve(factors.size() + children.size());
    gatheredFactors.insert(gatheredFactors.end(), factors.begin(), factors.end());
    gatheredFactors.insert(gatheredFactors.end(), childrenResults.begin(), childrenResults.end());

    // Do dense elimination step
    std::vector<Key> keyAsVector(1); keyAsVector[0] = key;
    std::pair<boost::shared_ptr<ConditionalType>, boost::shared_ptr<FactorType> > eliminationResult =
      function(gatheredFactors, keyAsVector);

    // Add conditional to BayesNet
    output->push_back(eliminationResult.first);

    // Return result
    return eliminationResult.second;
  }

  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  void EliminationTreeUnordered<BAYESNET,GRAPH>::Node::print(
    const std::string& str, const KeyFormatter& keyFormatter) const
  {
    std::cout << str << "(" << keyFormatter(key) << ")\n";
    BOOST_FOREACH(const sharedFactor& factor, factors) {
      if(factor)
        factor->print(str + "| ");
      else
        std::cout << str << "| null factor\n";
    }
  }


  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  EliminationTreeUnordered<BAYESNET,GRAPH>::EliminationTreeUnordered(const FactorGraphType& graph,
    const VariableIndexUnordered& structure, const OrderingUnordered& order)
  {
    gttic(ET_Create1);

    // Number of factors and variables - NOTE in the case of partial elimination, n here may
    // be fewer variables than are actually present in the graph.
    const size_t m = graph.size();
    const size_t n = order.size();

    static const size_t none = std::numeric_limits<size_t>::max();

    // Allocate result parent vector and vector of last factor columns
    std::vector<sharedNode> nodes(n);
    std::vector<size_t> parents(n, none);
    std::vector<size_t> prevCol(m, none);
    std::vector<bool> factorUsed(m, false);

    try {
      // for column j \in 1 to n do
      for (size_t j = 0; j < n; j++)
      {
        // Retrieve the factors involving this variable and create the current node
        const VariableIndexUnordered::Factors& factors = structure[order[j]];
        nodes[j] = boost::make_shared<Node>();
        nodes[j]->key = order[j];

        // for row i \in Struct[A*j] do
        BOOST_FOREACH(const size_t i, factors) {
          // If we already hit a variable in this factor, make the subtree containing the previous
          // variable in this factor a child of the current node.  This means that the variables
          // eliminated earlier in the factor depend on the later variables in the factor.  If we
          // haven't yet hit a variable in this factor, we add the factor to the current node.
          // TODO: Store root shortcuts instead of parents.
          if (prevCol[i] != none) {
            size_t k = prevCol[i];
            // Find root r of the current tree that contains k. Use raw pointers in computing the
            // parents to avoid changing the reference counts while traversing up the tree.
            size_t r = k;
            while (parents[r] != none)
              r = parents[r];
            // If the root of the subtree involving this node is actually the current node,
            // TODO: what does this mean?  forest?
            if (r != j) {
              // Now that we found the root, hook up parent and child pointers in the nodes.
              parents[r] = j;
              nodes[j]->children.push_back(nodes[r]);
            }
          } else {
            // Add the current factor to the current node since we are at the first variable in this
            // factor.
            nodes[j]->factors.push_back(graph[i]);
            factorUsed[i] = true;
          }
          prevCol[i] = j;
        }
      }
    } catch(std::invalid_argument& e) {
      // If this is thrown from structure[order[j]] above, it means that it was requested to
      // eliminate a variable not present in the graph, so throw a more informative error message.
      (void)e; // Prevent unused variable warning
      throw std::invalid_argument("EliminationTree: given ordering contains variables that are not involved in the factor graph");
    } catch(...) {
      throw;
    }

    // Find roots
    assert(parents.back() == none); // We expect the last-eliminated node to be a root no matter what
    for(size_t j = 0; j < n; ++j)
      if(parents[j] == none)
        roots_.push_back(nodes[j]);

    // Gather remaining factors
    for(size_t i = 0; i < m; ++i)
      if(!factorUsed[i])
        remainingFactors_.push_back(graph[i]);
  }

  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  EliminationTreeUnordered<BAYESNET,GRAPH>::EliminationTreeUnordered(
    const FactorGraphType& factorGraph, const OrderingUnordered& order)
  {
    gttic(ET_Create2);
    // Build variable index first
    const VariableIndexUnordered variableIndex(factorGraph);
    This temp(factorGraph, variableIndex, order);
    this->swap(temp); // Swap in the tree, and temp will be deleted
  }

  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  EliminationTreeUnordered<BAYESNET,GRAPH>&
    EliminationTreeUnordered<BAYESNET,GRAPH>::operator=(const EliminationTreeUnordered<BAYESNET,GRAPH>& other)
  {
    // Start by duplicating the tree.
    roots_ = treeTraversal::CloneForest(other);

    // Assign the remaining factors - these are pointers to factors in the original factor graph and
    // we do not clone them.
    remainingFactors_ = other.remainingFactors_;

    return *this;
  }

  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  std::pair<boost::shared_ptr<BAYESNET>, boost::shared_ptr<GRAPH> >
    EliminationTreeUnordered<BAYESNET,GRAPH>::eliminate(Eliminate function) const
  {
    // Allocate result
    boost::shared_ptr<BayesNetType> result = boost::make_shared<BayesNetType>();

    // Run tree elimination algorithm
    std::vector<sharedFactor> remainingFactors = inference::EliminateTree(result, *this, function);

    // Add remaining factors that were not involved with eliminated variables
    boost::shared_ptr<FactorGraphType> allRemainingFactors = boost::make_shared<FactorGraphType>();
    allRemainingFactors->push_back(remainingFactors_.begin(), remainingFactors_.end());
    allRemainingFactors->push_back(remainingFactors.begin(), remainingFactors.end());

    // Return result
    return std::make_pair(result, allRemainingFactors);
  }

  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  void EliminationTreeUnordered<BAYESNET,GRAPH>::print(const std::string& name, const KeyFormatter& formatter) const
  {
    treeTraversal::PrintForest(*this, name, formatter);
  }

  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  bool EliminationTreeUnordered<BAYESNET,GRAPH>::equals(const This& expected, double tol) const
  {
    // Depth-first-traversal stacks
    std::stack<sharedNode, std::vector<sharedNode> > stack1, stack2;

    // Add roots in sorted order
    {
      FastMap<Key,sharedNode> keys;
      BOOST_FOREACH(const sharedNode& root, this->roots_) { keys.insert(std::make_pair(root->key, root)); }
      typedef FastMap<Key,sharedNode>::value_type Key_Node;
      BOOST_FOREACH(const Key_Node& key_node, keys) { stack1.push(key_node.second); }
    }
    {
      FastMap<Key,sharedNode> keys;
      BOOST_FOREACH(const sharedNode& root, expected.roots_) { keys.insert(std::make_pair(root->key, root)); }
      typedef FastMap<Key,sharedNode>::value_type Key_Node;
      BOOST_FOREACH(const Key_Node& key_node, keys) { stack2.push(key_node.second); }
    }

    // Traverse, adding children in sorted order
    while(!stack1.empty() && !stack2.empty()) {
      // Pop nodes
      sharedNode node1 = stack1.top();
      stack1.pop();
      sharedNode node2 = stack2.top();
      stack2.pop();

      // Compare nodes
      if(node1->key != node2->key)
        return false;
      if(node1->factors.size() != node2->factors.size()) {
        return false;
      } else {
        for(Node::Factors::const_iterator it1 = node1->factors.begin(), it2 = node2->factors.begin();
          it1 != node1->factors.end(); ++it1, ++it2) // Only check it1 == end because we already returned false for different counts
        {
          if(*it1 && *it2) {
            if(!(*it1)->equals(**it2, tol))
              return false;
          } else if(*it1 && !*it2 || *it2 && !*it1) {
            return false;
          }
        }
      }

      // Add children in sorted order
      {
        FastMap<Key,sharedNode> keys;
        BOOST_FOREACH(const sharedNode& node, node1->children) { keys.insert(std::make_pair(node->key, node)); }
        typedef FastMap<Key,sharedNode>::value_type Key_Node;
        BOOST_FOREACH(const Key_Node& key_node, keys) { stack1.push(key_node.second); }
      }
      {
        FastMap<Key,sharedNode> keys;
        BOOST_FOREACH(const sharedNode& node, node2->children) { keys.insert(std::make_pair(node->key, node)); }
        typedef FastMap<Key,sharedNode>::value_type Key_Node;
        BOOST_FOREACH(const Key_Node& key_node, keys) { stack2.push(key_node.second); }
      }
    }

    // If either stack is not empty, the number of nodes differed
    if(!stack1.empty() || !stack2.empty())
      return false;

    return true;
  }

  /* ************************************************************************* */
  template<class BAYESNET, class GRAPH>
  void EliminationTreeUnordered<BAYESNET,GRAPH>::swap(This& other) {
    roots_.swap(other.roots_);
    remainingFactors_.swap(other.remainingFactors_);
  }


}
