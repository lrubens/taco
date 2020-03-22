#ifndef TACO_OPS_H
#define TACO_OPS_H

#include <vector>
#include <functional>
#include <map>

#include "taco/ir/ir.h"
#include "taco/util/collections.h"
#include "taco/index_notation/properties.h"
#include "taco/index_notation/index_notation.h"
#include "taco/index_notation/index_notation_nodes.h"

namespace taco {

class Op {

using opImpl = TensorOpNode::opImpl;
using algebraImpl = TensorOpNode::algebraImpl;

public:
  // Full construction
  Op(opImpl lowererFunc, algebraImpl algebraFunc, std::vector<Property> properties,
     std::map<std::vector<int>, opImpl> specialDefinitions = {})
  : name(util::uniqueName("Op")), lowererFunc(lowererFunc), algebraFunc(algebraFunc),
    properties(properties), regionDefinitions(specialDefinitions) {
  }

  Op(std::string name, opImpl lowererFunc, algebraImpl algebraFunc, std::vector<Property> properties,
     std::map<std::vector<int>, opImpl> specialDefinitions = {})
  : name(name), lowererFunc(lowererFunc), algebraFunc(algebraFunc), properties(properties),
    regionDefinitions(specialDefinitions) {
  }

  // Construct without specifying algebra
  Op(std::string name, opImpl lowererFunc, std::vector<Property> properties,
     std::map<std::vector<int>, opImpl> specialDefinitions  = {})
  : Op(name, lowererFunc, nullptr, properties, specialDefinitions) {
  }

  Op(opImpl lowererFunc, std::vector<Property> properties,
     std::map<std::vector<int>, opImpl> specialDefinitions = {})
  : Op(util::uniqueName("Op"), lowererFunc, nullptr, properties, specialDefinitions) {
  }

  // Construct without properties
  Op(std::string name, opImpl lowererFunc, algebraImpl algebraFunc,
     std::map<std::vector<int>, opImpl> specialDefinitions = {})
  : Op(name, lowererFunc, algebraFunc, {}, specialDefinitions) {
  }

  Op(opImpl lowererFunc, algebraImpl algebraFunc,
     std::map<std::vector<int>, opImpl> specialDefinitions = {})
  : Op(util::uniqueName("Op"), lowererFunc, algebraFunc, {}, specialDefinitions) {
  }

  // Construct without algebra or properties
  Op(std::string name, opImpl lowererFunc, std::map<std::vector<int>, opImpl> specialDefinitions = {})
  : Op(name, lowererFunc, nullptr, specialDefinitions) {
  }

  explicit Op(opImpl lowererFunc, std::map<std::vector<int>, opImpl> specialDefinitions = {})
  : Op(lowererFunc, nullptr, specialDefinitions) {
  }

  template<typename... IndexExprs>
  TensorOp operator()(IndexExprs&&... exprs) {
    std::vector<IndexExpr> actualArgs{exprs...};

    IterationAlgebra nodeAlgebra = algebraFunc == nullptr? inferAlgFromProperties(actualArgs): algebraFunc(actualArgs);

    TensorOpNode* op = new TensorOpNode(name, actualArgs, lowererFunc, nodeAlgebra, properties,
                                        regionDefinitions);

    return TensorOp(op);
  }

private:
  std::string name;
  opImpl lowererFunc;
  algebraImpl algebraFunc;
  std::vector<Property> properties;
  std::map<std::vector<int>, opImpl> regionDefinitions;

  IterationAlgebra inferAlgFromProperties(const std::vector<IndexExpr>& exprs) {
    if(properties.empty()) {
      return constructDefaultAlgebra(exprs);
    }
    return {};
  }

  // Constructs an algebra that iterates over the entire space
  static IterationAlgebra constructDefaultAlgebra(const std::vector<IndexExpr>& exprs) {
    if(exprs.empty()) return Region();

    IterationAlgebra tensorsRegions(exprs[0]);
    for(size_t i = 1; i < exprs.size(); ++i) {
      tensorsRegions = Union(tensorsRegions, exprs[i]);
    }

    IterationAlgebra background = Complement(tensorsRegions);
    return Union(tensorsRegions, background);
  }
};

}
#endif //TACO_OPS_H


