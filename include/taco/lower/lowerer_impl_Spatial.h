#ifndef TACO_LOWERER_IMPL_SPATIAL_H
#define TACO_LOWERER_IMPL_SPATIAL_H

#include <vector>
#include <map>
#include <set>
#include <memory>
#include <taco/index_notation/index_notation.h>

#include "taco/lower/iterator.h"
#include "taco/util/scopedset.h"
#include "taco/util/uncopyable.h"
#include "taco/ir_tags.h"
#include "taco/lower/lowerer_impl_dataflow.h"

namespace taco {
class LowererImplSpatial : public LowererImplDataflow {
public:
  LowererImplSpatial();
  virtual ~LowererImplSpatial() = default;

protected:
  /// Lower an assignment statement.
  ir::Stmt lowerAssignment(Assignment assignment) override;

  /// Lower an access expression.
  ir::Expr lowerAccess(Access access) override;

  ir::Stmt lowerForall(Forall forall) override;

  /// Retrieve the values array of the tensor var.
  ir::Expr getValuesArray(TensorVar) const override;

  /// Initialize temporary variables
  std::vector<ir::Stmt> codeToInitializeTemporary(Where where) override;

  ir::Stmt lowerDeclarativeSparse(MergeLattice lattice, IndexVar coordinateVar, Forall forall,
                             IndexStmt statement,
                             const std::set<Access>& reducedAccesses) ;

  ir::Stmt lowerForallDimension(Forall forall, Iterator iterator,
                                        std::vector<Iterator> locaters,
                                        std::vector<Iterator> inserters,
                                        std::vector<Iterator> appenders,
                                        std::set<Access> reducedAccesses,
                                        ir::Stmt recoveryStmt) override;

  ir::Stmt lowerForallPosition(Forall forall, Iterator iterator,
                               std::vector<Iterator> locators,
                               std::vector<Iterator> inserters,
                               std::vector<Iterator> appenders,
                               std::set<Access> reducedAccesses,
                               ir::Stmt recoveryStmt) override;

  /// Create statements to append coordinate to result modes.
  ir::Stmt appendCoordinate(std::vector<Iterator> appenders, ir::Expr coord) override;

  ir::Stmt declLocatePosVars(std::vector<Iterator> locators) override;

  /// Returns the segment of IR that generates the bitvectors from the FIFO crd arrays
  ir::Stmt generateIteratorBitVectors(IndexStmt statement, ir::Expr coordinate, IndexVar coordinateVar,
                                      MergePoint point,  std::map<Iterator, ir::Expr>& bvRawMap, std::map<Iterator, ir::Expr>& bvMap);

  /// Loads the crd arrays from DRAM into FIFOs
  ir::Stmt loadDRAMtoFIFO(IndexStmt statement, MergePoint point, std::map<Iterator, ir::Expr>& varMap);

  /// Generates the IR segment that counts up all of the bits in the bitvectors.
  /// This will be used in the result Pos array
  ir::Stmt generateIteratorAppendPositions(IndexStmt statement, ir::Expr coordinate, IndexVar coordinateVar, MergePoint point,
                                   std::vector<Iterator> appenders, std::map<Iterator, ir::Expr>& varMap, bool isUnion = false);

  /// Generate the compute loop that either takes the union or intersection of the multiple sparse iterators.
  ir::Stmt generateIteratorComputeLoop(Forall forall, IndexStmt statement, ir::Expr coordinate, IndexVar coordinateVar, MergePoint point,
                                       MergeLattice lattice, std::map<Iterator, ir::Expr>& varMap, const std::set<Access>& reducedAccesses, bool isUnion);

  bool hasSparseDRAMAccesses(IndexExpr expression);

  ir::Stmt hoistSparseDRAMAccesses(IndexExpr expression);

  ir::Stmt codeToInitializeIteratorVar(Iterator iterator, std::vector<Iterator> iterators, std::vector<Iterator> rangers,
                                       std::vector<Iterator> mergers, ir::Expr coordinate, IndexVar coordinateVar) override;

  ir::Stmt generateGlobalEnvironmentVars(IndexStmt stmt) override;
  ir::Stmt generateAccelEnvironmentVars() override;
  ir::Stmt addAccelEnvironmentVars() override;
  ir::Stmt codeToInitializePosAccumulators();

  ir::Stmt generateAppendPositions(std::vector<Iterator> appenders) override;
  ir::Stmt generateAppendPositionsForallPos(std::vector<Iterator> appenders, ir::Expr pos, ir::Expr coordinate);

  // TODO: add this to LowererImplImperative too
  std::vector<ir::Expr> getAllTemporaryModeArrays(Where where);

  ir::Stmt generateAppendCoordVar(std::vector<Iterator> appenders, ir::Expr coordinatePosVar);
  ir::Stmt generateOPMemLoads(Forall forall, ir::Stmt& forallBodyStmt, ir::Expr startBound, ir::Expr endBound, TensorVar tns);

  /// Helper method to hoist tensor accesses of FIFOs to teh correct locator loop position
  ir::Stmt generateHoistedtensorAccess(Forall forall);

  ir::Stmt generateResultStore(Forall forall, std::vector<Iterator> appenders, ir::Expr start, ir::Expr end);

  ir::Stmt generateOPResultHoist(Forall forall, ir::Stmt body);

  bool hasResult(std::vector<Iterator> appenders, TensorVar tensors);

    private:
  class Visitor;
  friend class Visitor;
  std::shared_ptr<Visitor> visitor;

  bool ignoreVectorize = false;

  int markAssignsAtomicDepth = 0;

  std::map<TensorVar, ir::Expr> sparseDRAMAccessMap;

  std::map<IndexVar, ir::Expr> indexVartoMaxVar;

  /// Map from indexvars to their max bits variable
  std::map<IndexVar, ir::Expr> indexVartoBitVarMap;

  std::vector<ir::Expr> posAccumulationVars;

  std::map<ir::Expr, std::vector<ir::Expr>> coordinateScanVarsMap;

  std::map<ir::Expr, ir::Expr> tensorPropertyVars;

  std::map<IndexStmt, ir::Expr> hoistedPosArr;
  std::map<IndexStmt, ir::Expr> hoistedCrdArr;
  std::map<IndexStmt, ir::Expr> hoistedValArr;
  std::map<IndexStmt, Iterator> hoistedArrIterator;


  std::map<ir::Expr, std::pair<ir::Expr, ir::Expr>> coordinateBounds;
  std::map<TensorVar, ir::Expr> hoistedAccessVars;
  std::vector<ir::Expr> parentLoopVar;

  // Get parent coordinate for scanners
  std::vector<ir::Expr> previousIteratorCoord;
};


} // namespace taco
#endif 