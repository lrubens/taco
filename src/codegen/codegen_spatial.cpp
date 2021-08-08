#include <iostream>
#include <fstream>
#include <dlfcn.h>
#include <algorithm>
#include <unordered_set>
#include <taco.h>

#include "taco/ir/ir_visitor.h"
#include "codegen_spatial.h"
#include "taco/error.h"
#include "taco/util/strings.h"
#include "taco/util/collections.h"
#include "taco/spatial.h"


using namespace std;

namespace taco {
namespace ir {

// Some helper functions
namespace {

// Include stdio.h for printf
// stdlib.h for malloc/realloc
// math.h for sqrt
// MIN preprocessor macro
// This *must* be kept in sync with taco_tensor_t.h
const string cHeaders =
  "#ifndef TACO_C_HEADERS\n"
  "#define TACO_C_HEADERS\n"
  "#include <stdio.h>\n"
  "#include <stdlib.h>\n"
  "#include <stdint.h>\n"
  "#include <stdbool.h>\n"
  "#include <math.h>\n"
  "#include <complex.h>\n"
  "#include <string.h>\n"
  "#define TACO_MIN(_a,_b) ((_a) < (_b) ? (_a) : (_b))\n"
  "#define TACO_MAX(_a,_b) ((_a) > (_b) ? (_a) : (_b))\n"
  "#define TACO_DEREF(_a) (((___context___*)(*__ctx__))->_a)\n"
  "#ifndef TACO_TENSOR_T_DEFINED\n"
  "#define TACO_TENSOR_T_DEFINED\n"
  "typedef enum { taco_mode_dense, taco_mode_sparse } taco_mode_t;\n"
  "typedef struct {\n"
  "  int32_t      order;         // tensor order (number of modes)\n"
  "  int32_t*     dimensions;    // tensor dimensions\n"
  "  int32_t      csize;         // component size\n"
  "  int32_t*     mode_ordering; // mode storage ordering\n"
  "  taco_mode_t* mode_types;    // mode storage types\n"
  "  uint8_t***   indices;       // tensor index data (per mode)\n"
  "  uint8_t*     vals;          // tensor values\n"
  "  int32_t      vals_size;     // values array size\n"
  "} taco_tensor_t;\n"
  "#endif\n"
  "int cmp(const void *a, const void *b) {\n"
  "  return *((const int*)a) - *((const int*)b);\n"
  "}\n"
  "int taco_binarySearchAfter(int *array, int arrayStart, int arrayEnd, int target) {\n"
  "  if (array[arrayStart] >= target) {\n"
  "    return arrayStart;\n"
  "  }\n"
  "  int lowerBound = arrayStart; // always < target\n"
  "  int upperBound = arrayEnd; // always >= target\n"
  "  while (upperBound - lowerBound > 1) {\n"
  "    int mid = (upperBound + lowerBound) / 2;\n"
  "    int midValue = array[mid];\n"
  "    if (midValue < target) {\n"
  "      lowerBound = mid;\n"
  "    }\n"
  "    else if (midValue > target) {\n"
  "      upperBound = mid;\n"
  "    }\n"
  "    else {\n"
  "      return mid;\n"
  "    }\n"
  "  }\n"
  "  return upperBound;\n"
  "}\n"
  "int taco_binarySearchBefore(int *array, int arrayStart, int arrayEnd, int target) {\n"
  "  if (array[arrayEnd] <= target) {\n"
  "    return arrayEnd;\n"
  "  }\n"
  "  int lowerBound = arrayStart; // always <= target\n"
  "  int upperBound = arrayEnd; // always > target\n"
  "  while (upperBound - lowerBound > 1) {\n"
  "    int mid = (upperBound + lowerBound) / 2;\n"
  "    int midValue = array[mid];\n"
  "    if (midValue < target) {\n"
  "      lowerBound = mid;\n"
  "    }\n"
  "    else if (midValue > target) {\n"
  "      upperBound = mid;\n"
  "    }\n"
  "    else {\n"
  "      return mid;\n"
  "    }\n"
  "  }\n"
  "  return lowerBound;\n"
  "}\n"
  "taco_tensor_t* init_taco_tensor_t(int32_t order, int32_t csize,\n"
  "                                  int32_t* dimensions, int32_t* mode_ordering,\n"
  "                                  taco_mode_t* mode_types) {\n"
  "  taco_tensor_t* t = (taco_tensor_t *) malloc(sizeof(taco_tensor_t));\n"
  "  t->order         = order;\n"
  "  t->dimensions    = (int32_t *) malloc(order * sizeof(int32_t));\n"
  "  t->mode_ordering = (int32_t *) malloc(order * sizeof(int32_t));\n"
  "  t->mode_types    = (taco_mode_t *) malloc(order * sizeof(taco_mode_t));\n"
  "  t->indices       = (uint8_t ***) malloc(order * sizeof(uint8_t***));\n"
  "  t->csize         = csize;\n"
  "  for (int32_t i = 0; i < order; i++) {\n"
  "    t->dimensions[i]    = dimensions[i];\n"
  "    t->mode_ordering[i] = mode_ordering[i];\n"
  "    t->mode_types[i]    = mode_types[i];\n"
  "    switch (t->mode_types[i]) {\n"
  "      case taco_mode_dense:\n"
  "        t->indices[i] = (uint8_t **) malloc(1 * sizeof(uint8_t **));\n"
  "        break;\n"
  "      case taco_mode_sparse:\n"
  "        t->indices[i] = (uint8_t **) malloc(2 * sizeof(uint8_t **));\n"
  "        break;\n"
  "    }\n"
  "  }\n"
  "  return t;\n"
  "}\n"
  "void deinit_taco_tensor_t(taco_tensor_t* t) {\n"
  "  for (int i = 0; i < t->order; i++) {\n"
  "    free(t->indices[i]);\n"
  "  }\n"
  "  free(t->indices);\n"
  "  free(t->dimensions);\n"
  "  free(t->mode_ordering);\n"
  "  free(t->mode_types);\n"
  "  free(t);\n"
  "}\n"
  "#endif\n";
} // anonymous namespace

template <class T>
static inline void acceptJoin(IRPrinter* printer, ostream& stream,
                              vector<T> nodes, string sep) {
  if (nodes.size() > 0) {
    nodes[0].accept(printer);
  }
  for (size_t i=1; i < nodes.size(); ++i) {
    if (isa<VarDecl>(nodes[i])) {
      stream << sep;
      nodes[i].accept(printer);
    }
  }
}

class SpatialEnvPrinter : public IRPrinter {
public:
  SpatialEnvPrinter(std::ostream& stream) : IRPrinter (stream) {};
  /// Compile a lowered function
  void printEnvArgs(Stmt stmt) {
    stmt.accept(this);
  }

protected:
  using IRPrinter::visit;

  void visit(const Block* op) {
    acceptJoin(this, stream, op->contents, ",\n");
  }

  void visit(const Allocate* op) {
    stream << "";
  }
  void visit(const VarDecl* op) {
    stream << "  ";
    op->var.accept(this);

    // TODO: case on type()
    stream << ":scala.Int";

    stream << " = ";
    op->rhs.accept(this);
  }
};

class SpatialRemoveFuncEnv : public IRPrinter {
public:
  SpatialRemoveFuncEnv(std::ostream& stream) : IRPrinter(stream) {};
  /// Compile a lowered function
  void removeFuncEnv(Stmt stmt) {
    stmt.accept(this);
  }

protected:
  using IRPrinter::visit;

  void visit(const FuncEnv* op) {
    stream << "";
  }

  void visit(const Allocate* op) {
    string elementType = (op->var.type().isInt()) ? "T" : printSpatialType(op->var.type(), false);

    stream << "  ";
    stream << "val ";
    op->var.accept(this);
    stream << " = ";
    auto memLoc = op->memoryLocation;

    bool needNumElements = true;
    bool retiming = false;
    switch(memLoc) {
      case MemoryLocation::SpatialSparseDRAMFalse:
      case MemoryLocation::SpatialSparseDRAM:
        stream << "SparseDRAM[" << elementType << "](";
        break;
      case MemoryLocation::SpatialDRAM:
        stream << "DRAM[" << elementType << "](";
        break;
      case MemoryLocation::SpatialSparseSRAM:
        stream << "SparseSRAM[" << elementType << "](";
        break;
      case MemoryLocation::SpatialSparseParSRAM:
        stream << "SparseParSRAM[" << elementType << "](bp)(";
        break;
      case MemoryLocation::SpatialSparseParSRAMSwizzle:
        stream << "SparseParSRAMSwizzle[" << elementType << "](";
        break;
      case MemoryLocation::SpatialFIFO:
        stream << "FIFO[" << elementType << "](";
        break;
      case MemoryLocation::SpatialFIFORetimed:
        stream << "FIFO[" << elementType << "](";
        retiming = true;
        break;
      case MemoryLocation::SpatialArgIn:
        stream << "ArgIn[" << elementType << "]";
        needNumElements = false;
        break;
      case MemoryLocation::SpatialArgOut:
        stream << "ArgOut[" << elementType << "]";
        needNumElements = false;
        break;
      case MemoryLocation::SpatialSRAM:
      default:
        // MemoryLocation::Default and MemoryLocation::SpatialSRAM
        stream << "SRAM[" << elementType << "](";
        break;
    }


    if (needNumElements) {
      parentPrecedence = MUL;
      op->num_elements.accept(this);
      parentPrecedence = TOP;
      if (memLoc == MemoryLocation::SpatialSparseParSRAM) {
        stream << ", true";
      }
      stream << ")";
    }
    if (retiming)
      stream << ".retiming";

    stream << endl;
  }

  string printSpatialType(Datatype type, bool is_ptr) {
    stringstream typeStr;
    switch (type.getKind()) {
      case Datatype::Bool: typeStr << "Bool"; break;
      case Datatype::UInt8: typeStr << "U8"; break;
      case Datatype::UInt16: typeStr << "U16"; break;
      case Datatype::UInt32: typeStr << "U32"; break;
      case Datatype::UInt64: typeStr << "U64"; break;
      case Datatype::UInt128: typeStr << "U28"; break;
      case Datatype::Int8: typeStr << "I8"; break;
      case Datatype::Int16: typeStr << "I16"; break;
      case Datatype::Int32: typeStr << "I32"; break;
      case Datatype::Int64: typeStr << "I64"; break;
      case Datatype::Int128: typeStr << "I28"; break;
      case Datatype::Float32:
      case Datatype::Float64: typeStr << "Flt"; break;
      case Datatype::Complex64:
      case Datatype::Complex128:
      case Datatype::Undefined: typeStr << "Undefined"; break;
    }
    stringstream ret;
    ret << typeStr.str();

    return ret.str();
  }
};

// find environment variables for generating declarations
class CodeGen_Spatial::FindEnvVars : public IRVisitor {
public:
  map<Expr, string, ExprCompare> varMap;

  CodeGen_Spatial *codeGen;

  // copy inputs and outputs into the map
  FindEnvVars(CodeGen_Spatial *codeGen)
  : codeGen(codeGen) {
  }

protected:
  using IRVisitor::visit;

  virtual void visit(const Var *op) {
    if (varMap.count(op) == 0) {
      varMap[op] = codeGen->genUniqueName(op->name);
    }
  }
};


// find variables for generating declarations
// generates a single var for each GetProperty
class CodeGen_Spatial::FindVars : public IRVisitor {
public:
  map<Expr, string, ExprCompare> varMap;

  // the variables for which we need to add declarations
  map<Expr, string, ExprCompare> varDecls;

  vector<Expr> localVars;

  // this maps from tensor, property, mode, index to the unique var
  map<tuple<Expr, TensorProperty, int, int>, string> canonicalPropertyVar;

  // this is for convenience, recording just the properties unpacked
  // from the output tensor so we can re-save them at the end
  map<tuple<Expr, TensorProperty, int, int>, string> outputProperties;

  // TODO: should replace this with an unordered set
  vector<Expr> outputTensors;
  vector<Expr> inputTensors;

  CodeGen_Spatial *codeGen;
  bool inEnv = false;

  // copy inputs and outputs into the map
  FindVars(vector<Expr> inputs, vector<Expr> outputs, CodeGen_Spatial *codeGen)
  : codeGen(codeGen) {
    for (auto v: inputs) {
      auto var = v.as<Var>();
      taco_iassert(var) << "Inputs must be vars in codegen";
      taco_iassert(varMap.count(var)==0) << "Duplicate input found in codegen";
      inputTensors.push_back(v);
      varMap[var] = var->name;
    }
    for (auto v: outputs) {
      auto var = v.as<Var>();
      taco_iassert(var) << "Outputs must be vars in codegen";
      taco_iassert(varMap.count(var)==0) << "Duplicate output found in codegen";
      outputTensors.push_back(v);
      varMap[var] = var->name;
    }
  }

protected:
  using IRVisitor::visit;

  virtual void visit(const FuncEnv *op) {
    inEnv = true;
    op->env.accept(this);
    inEnv = false;
  }

  virtual void visit(const Var *op) {
    if (varMap.count(op) == 0) {
      varMap[op] = codeGen->genUniqueName(op->name);
    }
  }

  virtual void visit(const Scope *op) {
    op->scopedStmt.accept(this);
    if (op->returnExpr.defined())
      op->returnExpr.accept(this);
  }

  virtual void visit(const VarDecl *op) {
    if (!util::contains(localVars, op->var)) {
      localVars.push_back(op->var);
    }
    // Needed for global environments
    if (inEnv && varMap.count(op->var) == 0) {
      varMap[op->var] = codeGen->genUniqueName(op->var.as<Var>()->name);
    }
    op->rhs.accept(this);
  }

  virtual void visit(const For *op) {
    if (!util::contains(localVars, op->var)) {
      localVars.push_back(op->var);
    }
    op->var.accept(this);
    op->start.accept(this);
    op->end.accept(this);
    op->increment.accept(this);
    op->contents.accept(this);
  }

  virtual void visit(const GetProperty *op) {
    if (!util::contains(inputTensors, op->tensor) &&
        !util::contains(outputTensors, op->tensor)) {
      // Don't create header unpacking code for temporaries
      //return;
    }

    if (varMap.count(op) == 0) {
      auto key =
              tuple<Expr,TensorProperty,int,int>(op->tensor,op->property,
                                                 (size_t)op->mode,
                                                 (size_t)op->index);
      if (canonicalPropertyVar.count(key) > 0) {
        varMap[op] = canonicalPropertyVar[key];
      } else {
        auto unique_name = codeGen->genUniqueName(op->name);
        canonicalPropertyVar[key] = unique_name;
        varMap[op] = unique_name;
        varDecls[op] = unique_name;
        if (util::contains(outputTensors, op->tensor)) {
          outputProperties[key] = unique_name;
        }
      }
    }
  }
};

CodeGen_Spatial::CodeGen_Spatial(std::ostream &dest, OutputKind outputKind, bool simplify)
    : CodeGen(dest, false, simplify, C), out(dest), outputKind(outputKind) {}

CodeGen_Spatial::~CodeGen_Spatial() {}

void CodeGen_Spatial::compile(Stmt stmt, bool isFirst) {
  varMap = {};
  localVars = {};

  if (isFirst) {
    // output the headers
    out << cHeaders;
  }
  out << endl;
  // generate code for the Stmt
  stmt.accept(this);
}

string CodeGen_Spatial::printFuncName(const Function *func, 
                              std::map<Expr, std::string, ExprCompare> inputMap, 
                              std::map<Expr, std::string, ExprCompare> outputMap) {
  stringstream ret;

  ret << "import spatial.dsl._" << endl;
  ret << "\n";
  ret << "import scala.collection.mutable.ListBuffer\n"
         "import spatial.metadata.memory.{Barrier => _,_}\n" << endl;
  ret << "class " << func->name << "_0 extends " << func->name << "()" << endl;
  ret << "\n";


  ret << "@spatial abstract class " << func->name << "(" << endl;

  stringstream argStream;
  // Add in global environments as part of arguments
  SpatialEnvPrinter(argStream).printEnvArgs(func->funcEnv);
  ret << argStream.str() << endl;

  // Parameters here
  string name = func->name.substr(0, func->name.find("_")); //

  ret << ") extends FluorineApp with " << name << "ComputeCheck {" << endl;
  ret << "  type T = Int\n" << endl;  // FIXME: make type changeable
  ret << "  // Code used for running multiple datasets\n";

  auto path = "";
  auto files = "";
  if (should_use_tensor_files()) {
    path = "\"/data/tensors/\"";
    files = "\"fb1k.tns\",\n";
  } else {
    path = "\"/data/mats/\"";
    files = "\"ibm32.mtx\",\n";
  }
  ret << "  private val rt = sys.env(\"TACO_TO_SPATIAL_HOME\") + " << path << "\n";
  ret << "  val argList = ListBuffer(\n";
  ret << "    " << files;
  ret << "    )\n";
  ret << "  private val absPaths = argList.map(m => rt + m)\n";
  ret << "  override def runtimeArgs: Args = absPaths.mkString(\" \")\n";
  // Needed for PIR/Spatial bug
  if (name == "Plus2CSFDSS")
    ret << "  override def pirArgs = super.pirArgs + \" --rtelm-unsafe\"\n";
  ret << "\n";
  ret << "def main(args: Array[String]): Unit =" << endl;
  return ret.str();
}

void CodeGen_Spatial::visit(const Function* func) {
  // if generating a header, protect the function declaration with a guard
  if (outputKind == HeaderGen) {
    out << "#ifndef TACO_GENERATED_" << func->name << "\n";
    out << "#define TACO_GENERATED_" << func->name << "\n";
  }

  int numYields = countYields(func);
  emittingCoroutine = (numYields > 0);
  funcName = func->name;
  labelCount = 0;

  resetUniqueNameCounters();
  FindVars inputVarFinder(func->inputs, {}, this);
  func->body.accept(&inputVarFinder);
  FindVars outputVarFinder({}, func->outputs, this);
  func->body.accept(&outputVarFinder);

  // output function declaration
  doIndent();
  out << printFuncName(func, inputVarFinder.varDecls, outputVarFinder.varDecls);

  // if we're just generating a header, this is all we need to do
  if (outputKind == HeaderGen) {
    out << ";\n";
    out << "#endif\n";
    return;
  }

  out << "{\n";

  indent++;

  // find all the vars that are not inputs or outputs and declare them
  resetUniqueNameCounters();
  FindVars varFinder(func->inputs, func->outputs, this);
  Block::make(func->funcEnv, func->accelEnv, func->accelEnvNoSimplify, func->body).accept(&varFinder);

  varMap = varFinder.varMap;
  localVars = varFinder.localVars;

  // Setup global environments
  SpatialRemoveFuncEnv(out).removeFuncEnv(func->funcEnv);
  out << endl;

  // Print variable declarations
  out << printDecls(varFinder.varDecls, func->inputs, func->outputs) << endl;

  if (emittingCoroutine) {
    out << printContextDeclAndInit(varMap, localVars, numYields, func->name)
        << endl;
  }

  //out << printInitMem(varFinder.varDecls, func->inputs, func->outputs) << endl;
  out << printInitArgs(func->funcEnv) << endl;

  doIndent();
  out << "Accel {\n";
  indent++;


  out << printDeclsAccel(varFinder.varDecls, func->inputs, func->outputs) << endl;

  // Setup Accel environments
  printEnv(func->accelEnv, varMap, true);
  out << endl;
  //printEnv(func->accelEnvNoSimplify, varMap);

  // output body
  print(func->body);

  // output repack only if we allocated memory
//  if (checkForAlloc(func))
//    out << endl << printPack(varFinder.outputProperties, func->outputs);

  if (emittingCoroutine) {
    out << printCoroutineFinish(numYields, funcName);
  }

  if (func->accelEnvNoSimplify.defined())
    printEnv(func->accelEnvNoSimplify, varMap);


  // Reformat output (store back into DRAM)
  out << printOutputStore(varFinder.varDecls, func->inputs, func->outputs);

  out << "  }\n"; // end Accel
  indent--;

  out << "\n";
  
  indent++;
  out << printOutputCheck(varFinder.varDecls, varFinder.outputProperties, func->inputs, func->outputs);
  //out << "assert(true)\n";

  out << "}\n";   // end main
  out << "}\n";   // end SpatialTest
  //out << "return 0;\n";
}


void CodeGen_Spatial::visit(const VarDecl* op) {
  if (emittingCoroutine) {
    doIndent();
    op->var.accept(this);
    parentPrecedence = Precedence::TOP;
    stream << " = ";
    op->rhs.accept(this);
    stream << ";";
    stream << endl;
  } else {
    doIndent();
    stream << "val";
    taco_iassert(isa<Var>(op->var));
    stream << " ";
    string varName = varNameGenerator.getUniqueName(util::toString(op->var));
    varNames.insert({op->var, varName});
    op->var.accept(this);
    parentPrecedence = Precedence::TOP;

    if (op->mem == MemoryLocation::SpatialReg) {
      stream << " = Reg[T](";
      op->rhs.accept(this);
      stream << ".to[T])";
      //stream << ";";
      stream << endl;
    } else {

      stream << " = ";
      op->rhs.accept(this);
      //stream << ";";
      stream << endl;
    }
  }
}

void CodeGen_Spatial::visit(const Yield* op) {
  printYield(op, localVars, varMap, labelCount, funcName);
}

// For Vars, we replace their names with the generated name,
// since we match by reference (not name)
void CodeGen_Spatial::visit(const Var* op) {
  taco_iassert(varMap.count(op) > 0) <<
      "Var " << op->name << " not found in varMap";
  out << varMap[op];
//  if (op->memoryLocation == MemoryLocation::SpatialReg)
//    out << ".value";
}

void CodeGen_Spatial::visit(const Malloc* op) {
  auto memLoc = op->memoryLocation;
  if (memLoc == MemoryLocation::SpatialSparseSRAM) {
    stream << "SparseSRAM[T](";
  } else if (memLoc == MemoryLocation::SpatialFIFO) {
    stream << "FIFO[T](";
  } else {
    // MemoryLocation::Default and MemoryLocation::SpatialSRAM
    stream << "SRAM[T](";
  }
  parentPrecedence = Precedence::TOP;
  op->size.accept(this);
  stream << ")";
}

static string genVectorizePragma(int width) {
  stringstream ret;
  ret << "#pragma clang loop interleave(enable) ";
  if (!width)
    ret << "vectorize(enable)";
  else
    ret << "vectorize_width(" << width << ")";

  return ret.str();
}

static string getParallelizePragma(LoopKind kind) {
  stringstream ret;
  ret << "#pragma omp parallel for schedule";
  switch (kind) {
    case LoopKind::Static:
      ret << "(static, 1)";
      break;
    case LoopKind::Dynamic:
      ret << "(dynamic, 1)";
      break;
    case LoopKind::Runtime:
      ret << "(runtime)";
      break;
    case LoopKind::Static_Chunked:
      ret << "(static)";
      break;
    default:
      break;
  }
  return ret.str();
}

static string getUnrollPragma(size_t unrollFactor) {
  return "#pragma unroll " + std::to_string(unrollFactor);
}

static string getAtomicPragma() {
  return "#pragma omp atomic";
}

// The next two need to output the correct pragmas depending
// on the loop kind (Serial, Static, Dynamic, Vectorized)
//
// Docs for vectorization pragmas:
// http://clang.llvm.org/docs/LanguageExtensions.html#extensions-for-loop-hint-optimizations
void CodeGen_Spatial::visit(const For* op) {

  // FIXME: [Spatial] See if this is the correct location
  doIndent();
  stream << keywordString("Foreach") << " (";

  auto start_lit = op->start.as<Literal>();
  if (start_lit != nullptr && !((start_lit->type.isInt() && 
                                start_lit->equalsScalar(0)) ||
                               (start_lit->type.isUInt() && 
                                start_lit->equalsScalar(0)))) {
    op->start.accept(this);
    stream << " to ";
  }

  parentPrecedence = BOTTOM;
  op->end.accept(this);
  stream << keywordString(" by ");

  auto lit = op->increment.as<Literal>();
  if (lit != nullptr && ((lit->type.isInt()  && lit->equalsScalar(1)) ||
                         (lit->type.isUInt() && lit->equalsScalar(1)))) {
    stream << "1";
  }
  else {
    op->increment.accept(this);
  }
  
  // Parallelization factor in spatial
  stream << " par ";
  op->numChunks.accept(this);

  stream << ") { ";
  op->var.accept(this);
  stream << " =>\n";

  op->contents.accept(this);
  doIndent();
  stream << "}";
  stream << endl;
}

void CodeGen_Spatial::visit(const ForScan* op) {

  // FIXME: [Spatial] See if this is the correct location
  doIndent();
  stream << keywordString("Foreach") << " (";


  op->scanner.accept(this);

  // FIXME: Is the parentPrecedence needed?
  //parentPrecedence = BOTTOM;

  stream << ") { ";
  op->caseType.accept(this);
  stream << " =>\n";

  op->contents.accept(this);
  doIndent();
  stream << "}";
  stream << endl;
}

void CodeGen_Spatial::visit(const While* op) {
  // it's not clear from documentation that clang will vectorize
  // while loops
  // however, we'll output the pragmas anyway
  if (op->kind == LoopKind::Vectorized) {
    doIndent();
    out << genVectorizePragma(op->vec_width);
    out << "\n";
  }

  IRPrinter::visit(op);
}

void CodeGen_Spatial::visit(const Reduce* op) {
  doIndent();
  stream << keywordString("Reduce") << "(" << op->reg << ")(";
  op->start.accept(this);
  stream << keywordString(" until ");
  parentPrecedence = BOTTOM;
  op->end.accept(this);
  stream << keywordString(" by ");
  op->increment.accept(this);
  stream << " par ";
  op->numChunks.accept(this);

  stream << ") { ";
  op->var.accept(this);
  stream << " => \n";

  op->contents.accept(this);
  stream << endl;
  doIndent();

  stream << "} { _ ";
  if (op->add)
    stream << "+";
  else
    stream << "-";
  stream << " _ }";
  stream << endl;
}

void CodeGen_Spatial::visit(const ReduceScan* op) {
  doIndent();
  stream << keywordString("Reduce") << "(" << op->reg << ")(";
  op->scanner.accept(this);
  stream << ") { ";
  op->caseType.accept(this);
  stream << " => \n";

  if (op->contents.defined())
    op->contents.accept(this);

  if (op->returnExpr.defined()) {
    doIndent();
    op->returnExpr.accept(this);
  }

  stream << endl;

  doIndent();
  stream << "} { _ ";
  if (op->add)
    stream << "+";
  else
    stream << "-";
  stream << " _ }";
  stream << endl;
}

void CodeGen_Spatial::visit(const GetProperty* op) {
  taco_iassert(varMap.count(op) > 0) <<
      "Property " << Expr(op) << " of " << op->tensor << " not found in varMap";
  out << varMap[op];
}

void CodeGen_Spatial::visit(const Min* op) {
  if (op->operands.size() == 1) {
    op->operands[0].accept(this);
    return;
  }
  for (size_t i=0; i<op->operands.size()-1; i++) {
    stream << "TACO_MIN(";
    op->operands[i].accept(this);
    stream << ",";
  }
  op->operands.back().accept(this);
  for (size_t i=0; i<op->operands.size()-1; i++) {
    stream << ")";
  }
}

void CodeGen_Spatial::visit(const Max* op) {
  if (op->operands.size() == 1) {
    op->operands[0].accept(this);
    return;
  }
  for (size_t i=0; i<op->operands.size()-1; i++) {
    stream << "TACO_MAX(";
    op->operands[i].accept(this);
    stream << ",";
  }
  op->operands.back().accept(this);
  for (size_t i=0; i<op->operands.size()-1; i++) {
    stream << ")";
  }
}

void CodeGen_Spatial::visit(const Allocate* op) {
  string elementType = (op->var.type() == tensorTypes) ? "T" : printSpatialType(op->var.type(), false);

  doIndent();
  stream << "val ";
  op->var.accept(this);
  stream << " = ";
  auto memLoc = op->memoryLocation;

  bool needNumElements = true;
  bool retiming = false;
  switch(memLoc) {
    case MemoryLocation::SpatialSparseDRAMFalse:
    case MemoryLocation::SpatialSparseDRAM:
      stream << "SparseDRAM[" << elementType << "](";
      break;
    case MemoryLocation::SpatialDRAM:
      stream << "DRAM[" << elementType << "](";
      break;
    case MemoryLocation::SpatialSparseSRAM:
      stream << "SparseSRAM[" << elementType << "](";
      break;
    case MemoryLocation::SpatialSparseParSRAM:
      stream << "SparseParSRAM[" << elementType << "](bp)(";
      break;
    case MemoryLocation::SpatialSparseParSRAMSwizzle:
      stream << "SparseParSRAMSwizzle[" << elementType << "](";
      break;
    case MemoryLocation::SpatialFIFO:
      stream << "FIFO[" << elementType << "](";
      break;
    case MemoryLocation::SpatialFIFORetimed:
      stream << "FIFO[" << elementType << "](";
      retiming = true;
      break;
    case MemoryLocation::SpatialArgIn:
      stream << "ArgIn[" << elementType << "]";
      needNumElements = false;
      break;
    case MemoryLocation::SpatialArgOut:
      stream << "ArgOut[" << elementType << "]";
      needNumElements = false;
      break;
    case MemoryLocation::SpatialSRAM:
    default:
      // MemoryLocation::Default and MemoryLocation::SpatialSRAM
      stream << "SRAM[" << elementType << "](";
      break;
  }


  if (needNumElements) {
    parentPrecedence = MUL;
    op->num_elements.accept(this);
    parentPrecedence = TOP;
    if (memLoc == MemoryLocation::SpatialSparseParSRAM) {
      stream << ", true";
    }
    stream << ")";
  }
  if (retiming)
    stream << ".retiming";

  stream << endl;
}

void CodeGen_Spatial::visit(const Sqrt* op) {
  taco_tassert(op->type.isFloat() && op->type.getNumBits() == 64) <<
      "Codegen doesn't currently support non-double sqrt";
  stream << "sqrt(";
  op->a.accept(this);
  stream << ")";
}

void CodeGen_Spatial::visit(const Assign* op) {
  if (op->use_atomics) {
    doIndent();
    stream << getAtomicPragma() << endl;
  }

  doIndent();
  op->lhs.accept(this);
  parentPrecedence = Precedence::TOP;
  stream << " = ";
  op->rhs.accept(this);

  //stream << ";";
  stream << endl;
}

void CodeGen_Spatial::visit(const Store* op) {
  if (op->use_atomics) {
    doIndent();
    stream << getAtomicPragma() << endl;
  }

  doIndent();
  op->arr.accept(this);

  if ((op->lhs_mem_loc == MemoryLocation::SpatialDRAM ||
      op->lhs_mem_loc == MemoryLocation::SpatialSRAM)
      //&&
//      !(op->rhs_mem_loc == MemoryLocation::SpatialReg ||
//        op->rhs_mem_loc == MemoryLocation::SpatialArgOut ||
//        op->rhs_mem_loc == MemoryLocation::SpatialArgIn)
        ) {
    stream << "(";
    parentPrecedence = Precedence::TOP;
    op->loc.accept(this);
    stream << ")";
  }

  if (op->rhs_mem_loc == MemoryLocation::SpatialDRAM) {
    stream << " load ";
  } else if (op->lhs_mem_loc == MemoryLocation::SpatialDRAM) {
    //stream << " store ";
    stream << " = ";
  } else if ((op->lhs_mem_loc == MemoryLocation::SpatialReg || op->lhs_mem_loc == MemoryLocation::SpatialArgOut ||
              op->lhs_mem_loc == MemoryLocation::SpatialArgIn)
              && (op->lhs_mem_loc == MemoryLocation::SpatialReg || op->lhs_mem_loc == MemoryLocation::SpatialArgOut ||
                  op->lhs_mem_loc == MemoryLocation::SpatialArgIn)) {
    stream << " := ";
  } else if (op->lhs_mem_loc == MemoryLocation::SpatialFIFO ||
             op->lhs_mem_loc == MemoryLocation::SpatialFIFORetimed) {
    stream << ".enq(";
  } else if (op->lhs_mem_loc == MemoryLocation::SpatialSparseDRAM || op->lhs_mem_loc == MemoryLocation::SpatialSparseDRAMFalse
            || op->lhs_mem_loc == MemoryLocation::SpatialSparseSRAM) {
    parentPrecedence = Precedence::TOP;
    stream << ".barrierWrite(";
    op->loc.accept(this);
    stream << ", ";
  } else {
    stream << " = ";
  }

  parentPrecedence = Precedence::TOP;
  op->data.accept(this);
  if (op->lhs_mem_loc == MemoryLocation::SpatialFIFO ||
      op->lhs_mem_loc == MemoryLocation::SpatialFIFORetimed) {
    stream << ")";
  } else if (op->lhs_mem_loc == MemoryLocation::SpatialSparseDRAM || op->lhs_mem_loc == MemoryLocation::SpatialSparseSRAM) {
    stream << ", Seq())";
  }
  stream << endl;
}

void CodeGen_Spatial::visit(const StoreBulk* op) {
  // Special case for stream_load_vec
  if (op->rhs_mem_loc == MemoryLocation::SpatialDRAM) {
    if (op->lhs_mem_loc == MemoryLocation::SpatialFIFORetimed || op->lhs_mem_loc == MemoryLocation::SpatialFIFO) {
      taco_iassert(isa<LoadBulk>(op->data)) << "If you're trying to load from DRAM to a FIFO, the data must be a "
                                               "bulk load. Currently it is not: " << op->data;
      auto load = op->data.as<LoadBulk>();

      doIndent();
      load->arr.accept(this);

      stream << " stream_load_vec(";
      load->locStart.accept(this);
      stream << ", ";
      load->locEnd.accept(this);
      stream << ", ";
      op->arr.accept(this);
      stream << ")" << endl;
      return;
    }
  }

  if (op->lhs_mem_loc == MemoryLocation::SpatialDRAM) {
    if (op->rhs_mem_loc == MemoryLocation::SpatialFIFORetimed || op->rhs_mem_loc == MemoryLocation::SpatialFIFO) {
      taco_iassert(isa<LoadBulk>(op->data)) << "If you're trying to load from DRAM to a FIFO, the data must be a "
                                               "bulk load. Currently it is not: " << op->data;
      auto load = op->data.as<LoadBulk>();

      doIndent();
      op->arr.accept(this);


      stream << " stream_store_vec(";
      load->locStart.accept(this);
      stream << ", ";
      load->arr.accept(this);
      stream << ", ";
      load->locEnd.accept(this);
      stream << ")" << endl;
      return;
    }
  }

  if (op->use_atomics) {
    doIndent();
    stream << getAtomicPragma() << endl;
  }

  doIndent();
  op->arr.accept(this);
  if (op->locStart.defined() && op->locEnd.defined()) {
    stream << "(";
    parentPrecedence = Precedence::TOP;
    op->locStart.accept(this);
    stream << "::";
    op->locEnd.accept(this);
    stream << ")";
  }

  if (op->rhs_mem_loc == MemoryLocation::SpatialDRAM) {
    stream << " load ";
  } else if (op->lhs_mem_loc == MemoryLocation::SpatialDRAM) {
    stream << " store ";
  } else {
    stream << " = ";
  }

  parentPrecedence = Precedence::TOP;
  op->data.accept(this);
  stream << endl;
}

void CodeGen_Spatial::visit(const MemStore* op) {
  doIndent();
  op->lhsMem.accept(this);
  stream << "(";
  parentPrecedence = Precedence::TOP;
  op->start.accept(this);
  stream << "::";
  op->start.accept(this);
  stream << "+";
  op->offset.accept(this);
  stream << ") store ";
  parentPrecedence = Precedence::TOP;
  op->rhsMem.accept(this);
  stream << endl;
}

void CodeGen_Spatial::visit(const Load* op) {
  parentPrecedence = Precedence::LOAD;
  op->arr.accept(this);
  switch (op->mem_loc) {
    case MemoryLocation::SpatialArgIn:
      break;
    case MemoryLocation::SpatialFIFORetimed:
    case MemoryLocation::SpatialFIFO:
      stream << ".deq";
      break;
    case MemoryLocation::SpatialSparseDRAM:
    case MemoryLocation::SpatialSparseDRAMFalse:
    case MemoryLocation::SpatialSparseParSRAM:
    case MemoryLocation::SpatialSparseParSRAMSwizzle:
    case MemoryLocation::SpatialSparseSRAM:
      stream << ".barrierRead(";
      parentPrecedence = Precedence::LOAD;
      op->loc.accept(this);
      stream << ", Seq())";
      break;
    default:
      stream << "(";
      parentPrecedence = Precedence::LOAD;
      op->loc.accept(this);
      stream << ")";
      break;

  }
}

void CodeGen_Spatial::visit(const Ternary* op) {
  taco_iassert(op->cond.defined());
  taco_iassert(op->then.defined());
  stream << "mux((";
  parentPrecedence = Precedence::TOP;
  op->cond.accept(this);
  stream << "), ";

  op->then.accept(this);
  stream << ", ";
  op->otherwise.accept(this);
  stream << ")";
}

void CodeGen_Spatial::visit(const LoadBulk* op) {
  parentPrecedence = Precedence::LOAD;
  op->arr.accept(this);
  stream << "(";
  parentPrecedence = Precedence::LOAD;
  op->locStart.accept(this);
  stream << "::";
  op->locEnd.accept(this);

  if (op->numChunks.defined()) {
    stream << " par ";
    op->numChunks.accept(this);
  }

  stream << ")";
}


void CodeGen_Spatial::visit(const MemLoad* op) {
  doIndent();
  op->lhsMem.accept(this);
  stream << "(";
  parentPrecedence = Precedence::TOP;
  op->start.accept(this);
  stream << "::";
  op->start.accept(this);
  stream << "+";
  op->offset.accept(this);
  stream << ")";
  stream << " load ";
  parentPrecedence = Precedence::TOP;
  op->rhsMem.accept(this);
  stream << endl;
}

void CodeGen_Spatial::visit(const Free* op) {
  parentPrecedence = Precedence::TOP;
}

void CodeGen_Spatial::generateShim(const Stmt& func, stringstream &ret) {
  const Function *funcPtr = func.as<Function>();

  ret << "int _shim_" << funcPtr->name << "(void** parameterPack) {\n";
  ret << "  return " << funcPtr->name << "(";

  size_t i=0;
  string delimiter = "";

  const auto returnType = funcPtr->getReturnType();
  if (returnType.second != Datatype()) {
    ret << "(void**)(parameterPack[0]), ";
    ret << "(char*)(parameterPack[1]), ";
    ret << "(" << returnType.second << "*)(parameterPack[2]), ";
    ret << "(int32_t*)(parameterPack[3])";

    i = 4;
    delimiter = ", ";
  }

  for (auto output : funcPtr->outputs) {
    auto var = output.as<Var>();
    auto cast_type = var->is_tensor ? "taco_tensor_t*"
    : printCType(var->type, var->is_ptr);

    ret << delimiter << "(" << cast_type << ")(parameterPack[" << i++ << "])";
    delimiter = ", ";
  }
  for (auto input : funcPtr->inputs) {
    auto var = input.as<Var>();
    auto cast_type = var->is_tensor ? "taco_tensor_t*"
    : printCType(var->type, var->is_ptr);
    ret << delimiter << "(" << cast_type << ")(parameterPack[" << i++ << "])";
    delimiter = ", ";
  }
  ret << ");\n";
  ret << "}\n";
}

string CodeGen_Spatial::unpackTensorPropertyAccel(string varname, const GetProperty* op,
                          bool is_output_prop) {
  stringstream ret;
  string indentation = "    ";

  // if load_local in GP set, do not emit at all
  if (op->load_local) {
    if (op->property == TensorProperty::Values)
      return "\n";
    return "";
  }


  auto tensor = op->tensor.as<Var>();
  string dims = "";
  if (op->property == TensorProperty::Values) {
    // for the values, it's in the last slot
    string memLoc = "";
    bool useBP = op->useBP;
    string memDims = "nnz_accel_max";
    string loadDims = "nnz_accel_max";
    bool retiming = false;
    bool emitMemBarrierTrue = false;
    bool emitMemBarrierFalse = false;

    switch(tensor->memoryLocation) {
      case MemoryLocation::SpatialFIFORetimed:
        memLoc = "FIFO";
        memDims = "16";
        useBP = false;
        retiming = true;
      case MemoryLocation::SpatialFIFO:
        memLoc = "FIFO";
        memDims = "16";
        useBP = false;
        break;
      case MemoryLocation::SpatialSparseDRAM:
        memLoc = "SparseDRAM";
        useBP = true;
        memDims = "nnz_max";
        break;
      case MemoryLocation::SpatialSparseDRAMFalse:
        memLoc = "SparseDRAM";
        useBP = true;
        memDims = "nnz_max";
        break;
      case MemoryLocation::SpatialSparseSRAM:
        memLoc = "SparseSRAM";
        break;
      case MemoryLocation::SpatialSparseParSRAM:
        memLoc = "SparseParSRAM";
        break;
      case MemoryLocation::SpatialSparseParSRAMSwizzle:
        memLoc = "SparseParSRAMSwizzle";
        emitMemBarrierTrue = true;
        break;
      case MemoryLocation::SpatialSRAM:
        memLoc = "SRAM";
        loadDims = "";
        for (int i = 1; i < op->index + 1; i++) {
          loadDims += tensor->name + to_string(i) + "_dimension";
          if (i < op->index) {
            loadDims += "*";
          }
        }
        break;
      case MemoryLocation::SpatialArgIn:
      case MemoryLocation::SpatialArgOut:
        return "";
        break;
      case MemoryLocation::SpatialReg:
      case MemoryLocation::SpatialDRAM:
        return "";
      default:
        break;
    }

    string bpString = (useBP)? "(bp)" : "";
    ret << indentation << "val " << varname << " = " << memLoc << "[T]" << bpString << "(";


    if (op->index == 0) {
      ret << "1";
      dims = "1";
    }
    else {
      for (int i = 1; i < op->index + 1; i++) {
        dims += tensor->name + to_string(i) + "_dimension";
        if (i < op->index) {
          dims += "*";
        }
      }
    }

    ret << memDims;
    if (emitMemBarrierTrue)
      ret << ", true";
    else if (emitMemBarrierFalse)
      ret << ", false";
    ret << ")";

    if (retiming)
      ret << ".retiming" << endl;
    else
      ret << endl;

    // Load from DRAM into FIFO
    // TODO: case based on FIFO or SRAM
    if (!is_output_prop ) {
      switch(tensor->memoryLocation) {
        case MemoryLocation::SpatialFIFORetimed:
        case MemoryLocation::SpatialFIFO:
          ret << indentation << varname << "_dram stream_load_vec(0, " << dims << ", " << varname << ")" << endl;
          break;
        case MemoryLocation::SpatialSparseDRAMFalse:
        case MemoryLocation::SpatialSparseDRAM:
          ret << indentation << varname << ".alias = " << varname << "_dram" << endl;
          break;
        case MemoryLocation::SpatialSparseSRAM:
        case MemoryLocation::SpatialSparseParSRAM:
        case MemoryLocation::SpatialSparseParSRAMSwizzle:
        case MemoryLocation::SpatialSRAM:
          ret << indentation << varname << " load " << varname << "_dram(0::" << dims << " par ip)" << endl;
          break;
        case MemoryLocation::SpatialDRAM:
        default:
          break;
      }
    } else {
      switch(tensor->memoryLocation) {
        case MemoryLocation::SpatialSparseDRAM:
          ret << indentation << varname << ".alias = " << varname << "_dram" << endl;
          break;
        case MemoryLocation::SpatialSparseSRAM:
        case MemoryLocation::SpatialFIFO:
        case MemoryLocation::SpatialSRAM:
        case MemoryLocation::SpatialDRAM:
        default:
          break;
      }
    }
    ret << endl;
    return (tensor->memoryLocation == MemoryLocation::SpatialReg) ? "" : ret.str();
  } else if (op->property == TensorProperty::ValuesSize) {
    ret << indentation << "int " << varname << " = " << tensor->name << "->vals_size;\n";

    return ret.str();
  }

  string tp;

  // for a Dense level, nnz is an int
  // for a Fixed level, ptr is an int
  // all others are int*
  if (op->property == TensorProperty::Dimension) {
    return "";
  } else {
    taco_iassert(op->property == TensorProperty::Indices);

    auto nm = op->index;

    if (tensor->memoryLocation == MemoryLocation::SpatialSparseDRAMFalse && nm) {
      return "";
    }

    string memLoc = "";
    string loadDims = "nnz_accel_max";
    string memDims = "nnz_accel_max";
    bool useBP = op->useBP;
    bool retiming = false;
    string dims = "";
    for (int i = 1; i <= op->mode + 1; i++) {
      dims += tensor->name + to_string(i) + "_dimension";
      if (i <= op->mode) {
        dims += "*";
      }
    }
    bool emitMemBarrierFalse = false;

    switch(tensor->memoryLocation) {
      case MemoryLocation::SpatialFIFORetimed:
        memLoc = (nm == 1) ? "FIFO" : "SRAM"; // Needs to be SparseSRAM if there is a (pseudo-random inner loop accesses)
        memDims = (nm == 1) ? "16" : "nnz_accel_max";
        loadDims = (nm == 1) ? dims :  (op->mode == 0) ? "ip" : "(" + tensor->name + to_string(std::max(op->mode, 1)) + "_dimension + 1)";
        useBP = false;
        retiming = (nm == 1);
        break;
      case MemoryLocation::SpatialFIFO:
        memLoc = (nm == 1) ? "FIFO" : "SRAM"; // Needs to be SparseSRAM if there is a (pseudo-random inner loop accesses)
        memDims = (nm == 1) ? "16" : "nnz_accel_max";
        loadDims = (nm == 1) ? dims :  (op->mode == 0) ? "ip" : "(" + tensor->name + to_string(std::max(op->mode, 1)) + "_dimension + 1)";
        useBP = false;
        break;
      case MemoryLocation::SpatialSparseDRAM:
        // nm == 1 means it's the crd array
        memLoc = (is_output_prop) ? "SparseDRAM" : (nm == 1) ? "" : "SRAM";
        loadDims = (is_output_prop) ? "nnz_max" : (nm == 1) ? dims :  (op->mode == 0) ? "ip" : "(" + tensor->name + to_string(std::max(op->mode, 1)) + "_dimension + 1)";
        useBP = is_output_prop || nm != 0;
        break;
      case MemoryLocation::SpatialSparseDRAMFalse:
        memLoc = "SparseDRAM";
        useBP = true;
        memDims = "nnz_accel_max";
        emitMemBarrierFalse = true;
        break;
      case MemoryLocation::SpatialSparseSRAM:
        // nm == 1 means it's the crd array
        memLoc = (nm == 1) ? "SparseSRAM" : "SRAM";
        break;
      case MemoryLocation::SpatialSRAM:
        memLoc = "SRAM";
        break;
      case MemoryLocation::SpatialDRAM:
      default:
        break;
    }

    if (memLoc != "") {
      string bpString = (useBP) ? "(bp)" : "";
      string memBar = (emitMemBarrierFalse) ? ", false" : "";
      ret << indentation << "val " << varname << " = " << memLoc << "[T]" << bpString << "(" << memDims << memBar << ")";
    }
    if (retiming)
      ret << ".retiming" << endl;
    else
      ret << endl;

    // FIXME: Fixed size right now, should be max(NNZ)
    if (!is_output_prop) {
      if (memLoc == "FIFO") {
        ret << indentation << varname << "_dram stream_load_vec(0, " << loadDims << ", " << varname << ")" << endl;
      } else if (memLoc == "SRAM" || memLoc == "SparseSRAM" || memLoc == "SparseParSRAM") {
        // FIXME: need dimension for transfer here
        ret << indentation << varname << " load " << varname << "_dram(0::" << loadDims << " par ip)" << endl;
      } else if (memLoc == "SparseDRAM") {
        ret << indentation << varname << ".alias = " << varname << "_dram" << endl;
      }
    } else {
      if (memLoc == "SparseDRAM") {
        ret << indentation << varname << ".alias = " << varname << "_dram" << endl;
      }
    }


  }

  return ret.str();
  
}

string CodeGen_Spatial::unpackTensorProperty(string varname, const GetProperty* op,
                            bool is_output_prop) {
  stringstream ret;
  string indent = "  ";
  ret << indent;

  auto tensor = op->tensor.as<Var>();
  if (op->property == TensorProperty::Values) {
    string loc = "DRAM";
    bool emitDim = true;
    bool useDRAM = true;
    string dim = "(nnz_max)";
    if (tensor->memoryLocation == MemoryLocation::SpatialDRAM) {
      useDRAM = false;
    }
    else if (tensor->memoryLocation == MemoryLocation::SpatialSRAM) {
      //loc = "SRAM";
//      dim = "(";
//      for (int i = 1; i < op->index + 1; i++) {
//        dim += tensor->name + to_string(i) + "_dimension";
//        if (i < op->index) {
//          dim += "*";
//        }
//      }
//      dim += ")";
    } else if (tensor->memoryLocation == MemoryLocation::SpatialReg) {
      useDRAM = false;
    } else if (tensor->memoryLocation == MemoryLocation::SpatialArgIn || tensor->memoryLocation == MemoryLocation::SpatialArgOut) {
      emitDim = false;
      useDRAM = false;
      if (is_output_prop)
        loc = "ArgOut";
      else
        loc = "ArgIn";
    } else {
      if (op->index == 0)
        dim = "(1)";
    }

//    if (op->load_local)
//      useDRAM = false;

    ret << "val " << varname;
    if (useDRAM)
      ret << "_dram";
    ret << " = " << loc << "[T]";
    if (emitDim)
      ret << dim;

    ret << endl;

    // For formatting reasons
    if (!is_output_prop)
      ret << endl;
  } else if (op->property == TensorProperty::ValuesSize) {
    ret << "int " << varname << " = " << tensor->name << "->vals_size;\n";
  }
  // for a Dense level, nnz is an int
  // for a Fixed level, ptr is an int
  // all others are int*
  else if (op->property == TensorProperty::Dimension) {
    string loc = (is_output_prop) ? "ArgIn" : "ArgIn";
    ret << "val " << varname << " = " << loc << "[T]" << endl;
    //ret << "val " << varname << "_dram = " << op->index << endl;
  } else {
    taco_iassert(op->property == TensorProperty::Indices);

    string loc = "DRAM";
    if (tensor->memoryLocation == MemoryLocation::SpatialSRAM) {
      loc = "SRAM";
    } else if (tensor->memoryLocation == MemoryLocation::SpatialReg) {
      if (is_output_prop)
        loc = "ArgOut";
      else
        loc = "ArgIn";
    }
    bool useDRAM = true;

    useDRAM = (is_output_prop) || !((op->index == 1 &&
      (tensor->memoryLocation == MemoryLocation::SpatialSparseDRAM || tensor->memoryLocation == MemoryLocation::SpatialSparseDRAMFalse)));

    // FIXME: should detect for tensor contraction

    auto dramVar = (useDRAM) ? "_dram" : "";
    ret << "val " << varname << dramVar << " = ";
    ret << loc << "[T](nnz_max)\n";
  }


  if (op->property == TensorProperty::Values)
    ret << endl;

  return ret.str();
}

// helper to print declarations
string CodeGen_Spatial::printDecls(map<Expr, string, ExprCompare> varMap,
                           vector<Expr> inputs, vector<Expr> outputs) {
  stringstream ret;
  unordered_set<string> propsAlreadyGenerated;

  vector<const GetProperty*> sortedProps;
  vector<Expr> props;

  // Add properties to sortedProps and keep track of dimension names
  vector<string> dimPropNames;
  for (auto const& p: varMap) {
    if (p.first.as<GetProperty>()) {
      auto getProperty = p.first.as<GetProperty>();
      sortedProps.push_back(p.first.as<GetProperty>());
      if (getProperty->property == TensorProperty::Dimension)
        dimPropNames.push_back(getProperty->name);
    }
  }

  // sort the properties in order to generate them in a canonical order
  sort(sortedProps.begin(), sortedProps.end(),
       [&](const GetProperty *a,
           const GetProperty *b) -> bool {
         // first, use a total order of outputs,inputs
         auto a_it = find(outputs.begin(), outputs.end(), a->tensor);
         auto b_it = find(outputs.begin(), outputs.end(), b->tensor);
         auto a_pos = distance(outputs.begin(), a_it);
         auto b_pos = distance(outputs.begin(), b_it);
         if (a_it == outputs.end())
           a_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  a->tensor));
         if (b_it == outputs.end())
           b_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  b->tensor));

         // if total order is same, have to do more, otherwise we know
         // our answer
         if (a_pos != b_pos)
           return a_pos < b_pos;

         // if they're different properties, sort by property
         if (a->property != b->property)
           return a->property < b->property;

         // now either the mode gives order, or index #
         if (a->mode != b->mode)
           return a->mode < b->mode;

         return a->index < b->index;
       });

  // Add dimensions for position arrays which are not used by the lowerer
  // This is needed for hardware metadata
  string prevGpName = "";
  for (auto const& getProperty: sortedProps) {
    if (getProperty->property == TensorProperty::Indices && getProperty->index == 0) {
      const Expr tensor = getProperty->tensor;
      const Expr dimensionProperty = ir::GetProperty::make(tensor,
                                                           TensorProperty::Dimension, getProperty->mode);

      if (prevGpName == dimensionProperty.as<GetProperty>()->name) {
        const Expr cscDimProperty = ir::GetProperty::make(tensor,
                                                          TensorProperty::Dimension, getProperty->mode - 1);
        if (std::find(dimPropNames.begin(), dimPropNames.end(), cscDimProperty.as<GetProperty>()->name) == dimPropNames.end())
          props.push_back(cscDimProperty);
      } else if (std::find(dimPropNames.begin(), dimPropNames.end(), dimensionProperty.as<GetProperty>()->name) == dimPropNames.end()){
        props.push_back(dimensionProperty);
      }
    }
    prevGpName = getProperty->name;
  }

  for (auto const& prop: props) {
    if (varMap.count(prop) == 0) {
      auto unique_name = genUniqueName(prop.as<GetProperty>()->name);
      varMap[prop] = unique_name;
    }
    sortedProps.push_back(prop.as<GetProperty>());
  }

  // sort the properties in order to generate them in a canonical order
  sort(sortedProps.begin(), sortedProps.end(),
       [&](const GetProperty *a,
           const GetProperty *b) -> bool {
         // first, use a total order of outputs,inputs
         auto a_it = find(outputs.begin(), outputs.end(), a->tensor);
         auto b_it = find(outputs.begin(), outputs.end(), b->tensor);
         auto a_pos = distance(outputs.begin(), a_it);
         auto b_pos = distance(outputs.begin(), b_it);
         if (a_it == outputs.end())
           a_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  a->tensor));
         if (b_it == outputs.end())
           b_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  b->tensor));

         // if total order is same, have to do more, otherwise we know
         // our answer
         if (a_pos != b_pos)
           return a_pos < b_pos;

         // if they're different properties, sort by property
         if (a->property != b->property)
           return a->property < b->property;

         // now either the mode gives order, or index #
         if (a->mode != b->mode)
           return a->mode < b->mode;

         return a->index < b->index;
       });

  for (auto prop: sortedProps) {
    bool isOutputProp = (find(outputs.begin(), outputs.end(),
                              prop->tensor) != outputs.end());
    auto var = prop->tensor.as<Var>();
    if (var->is_parameter) {
      if (isOutputProp) {
        ret << "  " << printTensorProperty(varMap[prop], prop, false) << ";" << endl;
      } else {
        break;
      }
    } else {
      ret << unpackTensorProperty(varMap[prop], prop, isOutputProp);
    }
    propsAlreadyGenerated.insert(varMap[prop]);
  }
  ret << endl;

  // Output initMem(...) function for Spatial App
  ret << "  initMem[T](";
  for (int i = 0; i < (int)sortedProps.size(); i++) {
    auto prop = sortedProps[i];
    bool isOutputProp = (find(outputs.begin(), outputs.end(),
                              prop->tensor) != outputs.end());

    auto var = prop->tensor.as<Var>();
    if (!var->is_parameter) {
      ret << outputInitMemArgs(varMap[prop], prop, isOutputProp, i == (int)sortedProps.size() - 1);
    }
    propsAlreadyGenerated.insert(varMap[prop]);
  }
  ret << ")" << endl;
  return ret.str();
}

// helper to print declarations
string CodeGen_Spatial::printDeclsAccel(map<Expr, string, ExprCompare> varMap,
                           vector<Expr> inputs, vector<Expr> outputs) {
  stringstream ret;
  unordered_set<string> propsAlreadyGenerated;

  vector<const GetProperty*> sortedProps;

  for (auto const& p: varMap) {
    if (p.first.as<GetProperty>()) {
      auto getProperty = p.first.as<GetProperty>();
      sortedProps.push_back(getProperty);
    }
  }
  // sort the properties in order to generate them in a canonical order
  sort(sortedProps.begin(), sortedProps.end(),
       [&](const GetProperty *a,
           const GetProperty *b) -> bool {
         // first, use a total order of outputs,inputs
         auto a_it = find(outputs.begin(), outputs.end(), a->tensor);
         auto b_it = find(outputs.begin(), outputs.end(), b->tensor);
         auto a_pos = distance(outputs.begin(), a_it);
         auto b_pos = distance(outputs.begin(), b_it);
         if (a_it == outputs.end())
           a_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  a->tensor));
         if (b_it == outputs.end())
           b_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  b->tensor));

         // if total order is same, have to do more, otherwise we know
         // our answer
         if (a_pos != b_pos)
           return a_pos < b_pos;

         // if they're different properties, sort by property
         if (a->property != b->property)
           return a->property < b->property;

         // now either the mode gives order, or index #
         if (a->mode != b->mode)
           return a->mode < b->mode;

         return a->index < b->index;
       });

  for (auto prop: sortedProps) {
    bool isOutputProp = (find(outputs.begin(), outputs.end(),
                              prop->tensor) != outputs.end());
    
    auto var = prop->tensor.as<Var>();
    if (!var->is_parameter) {
      ret << unpackTensorPropertyAccel(varMap[prop], prop, isOutputProp);
    }
    propsAlreadyGenerated.insert(varMap[prop]);
  }

  return ret.str();
}

// helper to print declarations
string CodeGen_Spatial::printInitMem(map<Expr, string, ExprCompare> varMap,
                           vector<Expr> inputs, vector<Expr> outputs) {
  stringstream ret;
  unordered_set<string> propsAlreadyGenerated;

  vector<const GetProperty*> sortedProps;
  for (auto const& p: varMap) {
    if (p.first.as<GetProperty>()) {
      auto getProperty = p.first.as<GetProperty>();
      sortedProps.push_back(getProperty);
    }
  }
//  vector<Expr> props;
//
//  for (auto const& p: varMap) {
//    if (p.first.as<GetProperty>()) {
//      auto getProperty = p.first.as<GetProperty>();
//
//      if (getProperty->property == TensorProperty::Indices && getProperty->index == 0) {
//        const Expr tensor = getProperty->tensor;
//        const Expr dimensionProperty = ir::GetProperty::make(tensor,
//                                                             TensorProperty::Dimension, getProperty->mode);
//        if (sortedProps.size() > 0 && sortedProps.back()->name == dimensionProperty.as<GetProperty>()->name){
//          const Expr cscDimProperty = ir::GetProperty::make(tensor,
//                                                            TensorProperty::Dimension, getProperty->mode-1);
//          props.push_back(cscDimProperty);
//        } else {
//          props.push_back(dimensionProperty);
//        }
//      }
//
//      sortedProps.push_back(p.first.as<GetProperty>());
//    }
//  }
//
//  for (auto const& prop: props) {
//    if (varMap.count(prop) == 0) {
//      auto unique_name = genUniqueName(prop.as<GetProperty>()->name);
//      varMap[prop] = unique_name;
//    }
//    sortedProps.push_back(prop.as<GetProperty>());
//  }

  // sort the properties in order to generate them in a canonical order
  sort(sortedProps.begin(), sortedProps.end(),
       [&](const GetProperty *a,
           const GetProperty *b) -> bool {
         // first, use a total order of outputs,inputs
         auto a_it = find(outputs.begin(), outputs.end(), a->tensor);
         auto b_it = find(outputs.begin(), outputs.end(), b->tensor);
         auto a_pos = distance(outputs.begin(), a_it);
         auto b_pos = distance(outputs.begin(), b_it);
         if (a_it == outputs.end())
           a_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  a->tensor));
         if (b_it == outputs.end())
           b_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  b->tensor));

         // if total order is same, have to do more, otherwise we know
         // our answer
         if (a_pos != b_pos)
           return a_pos < b_pos;

         // if they're different properties, sort by property
         if (a->property != b->property)
           return a->property < b->property;

         // now either the mode gives order, or index #
         if (a->mode != b->mode)
           return a->mode < b->mode;

         return a->index < b->index;
       });

  // Output initMem(...) function for Spatial App  
  ret << "  initMem[T](";
  for (int i = 0; i < (int)sortedProps.size(); i++) {
    auto prop = sortedProps[i];
    bool isOutputProp = (find(outputs.begin(), outputs.end(),
                              prop->tensor) != outputs.end());
    
    auto var = prop->tensor.as<Var>();
    if (!var->is_parameter) {
      ret << outputInitMemArgs(varMap[prop], prop, isOutputProp, i == (int)sortedProps.size() - 1);
    }
    propsAlreadyGenerated.insert(varMap[prop]);
  }
  ret << ")" << endl;

  return ret.str();
}

string CodeGen_Spatial::outputInitMemArgs(string varname, const GetProperty* op,
                          bool is_output_prop, bool last) {
  stringstream ret;
  string indentation = "";
  ret << indentation;

  auto tensor = op->tensor.as<Var>();
  if (op->property == TensorProperty::Values && op->index == 0) {
    if (tensor->memoryLocation == MemoryLocation::SpatialReg ||
        tensor->memoryLocation == MemoryLocation::SpatialArgIn ||
        tensor->memoryLocation == MemoryLocation::SpatialArgOut)
      ret << "1, " << varname;
    else
      ret << "1, " << varname << "_dram";
  } else if (op->property == TensorProperty::Values) {
    if (tensor->memoryLocation == MemoryLocation::SpatialDRAM)
      ret << varname;
    else
      ret << varname << "_dram";
  } else if (op->property == TensorProperty::Dimension) {
    ret << varname;
  } else if (op->property == TensorProperty::Indices) {
    if (!is_output_prop && op->index == 1 && tensor->memoryLocation == MemoryLocation::SpatialSparseDRAM)
      ret << varname;
    else if (!is_output_prop && op->index == 1 && tensor->memoryLocation == MemoryLocation::SpatialSparseDRAMFalse)
      ret << varname;
    else
      ret << varname << "_dram";
  }

//  if (is_output_prop && op->property != TensorProperty::Dimension) {
//    ret << ", gold_" << varname << "_dram";
//  }

  if (!last) {
    ret << ", ";
  } else {
    // Used for running multiple datasets in TACO to Spatial apps
    ret << ", args";
  }

  return ret.str();
  
}

string CodeGen_Spatial::printInitArgs(Stmt stmt) {
  stringstream ret;
  string indentation = "  ";


  struct GetArgInVarnames : IRVisitor {
    using IRVisitor::visit;
    std::vector<std::string> varnames;
    bool isArgIn = false;

    void visit(const Var* var) {
      if (isArgIn)
        varnames.push_back(var->name);
      isArgIn = false;
    }

    void visit(const Allocate* allocate) {
      if (allocate->memoryLocation == MemoryLocation::SpatialArgIn) {

        isArgIn = true;
      }
      allocate->var.accept(this);
    }

    std::vector<std::string> getVarNames(Stmt stmt) {
      stmt.accept(this);
      return varnames;
    }
  };
  auto visitor = GetArgInVarnames();
  auto args = visitor.getVarNames(stmt);

  if (!args.empty()) {
    ret << indentation;
    ret << "initArgs(";
    for (auto& arg : args) {
      ret << arg << ", ";
    }
    ret << "args)\n";
  }
  return ret.str();
}

// helper to print output store
string CodeGen_Spatial::printOutputCheck(map<Expr, string, ExprCompare> varMap,
                                         map<tuple<Expr, TensorProperty, int, int>, string> outputProperties,
                                         vector<Expr> inputs, vector<Expr> outputs) {
    stringstream ret;
    unordered_set<string> propsAlreadyGenerated;

    vector<const GetProperty*> sortedProps;

    // Add properties to sortedProps and keep track of dimension names
    for (auto const& p: varMap) {
      if (p.first.as<GetProperty>()) {
        if (util::contains(outputs, p.first.as<GetProperty>()->tensor)) {
          auto getProperty = p.first.as<GetProperty>();
          sortedProps.push_back(p.first.as<GetProperty>());
        }
      }
    }

    // sort the properties in order to generate them in a canonical order
    sort(sortedProps.begin(), sortedProps.end(),
         [&](const GetProperty *a,
             const GetProperty *b) -> bool {
           // first, use a total order of outputs,inputs
           auto a_it = find(outputs.begin(), outputs.end(), a->tensor);
           auto b_it = find(outputs.begin(), outputs.end(), b->tensor);
           auto a_pos = distance(outputs.begin(), a_it);
           auto b_pos = distance(outputs.begin(), b_it);
           if (a_it == outputs.end())
             a_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                    a->tensor));
           if (b_it == outputs.end())
             b_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                    b->tensor));

           // if total order is same, have to do more, otherwise we know
           // our answer
           if (a_pos != b_pos)
             return a_pos < b_pos;

           // if they're different properties, sort by property
           if (a->property != b->property)
             return a->property < b->property;

           // now either the mode gives order, or index #
           if (a->mode != b->mode)
             return a->mode < b->mode;

           return a->index < b->index;
         });

  ret << "  checkOutput[T](";

  for (int i = 0; i < (int)sortedProps.size(); i++) {
    auto prop = sortedProps[i];
    bool isOutputProp = (find(outputs.begin(), outputs.end(),
                              prop->tensor) != outputs.end());

    auto var = prop->tensor.as<Var>();
    if (!var->is_parameter && isOutputProp) {
      ret << outputCheckOutputArgs(varMap[prop], prop, isOutputProp, i == (int)sortedProps.size() - 1);
    }
  }
  ret << ")" << endl;
  return ret.str();
}

string CodeGen_Spatial::outputCheckOutputArgs(string varname, const GetProperty* op,
                                              bool is_output_prop, bool last) {
  stringstream ret;
  ret << "";

  auto tensor = op->tensor.as<Var>();
  if (op->property == TensorProperty::Values && index == 0) {
    if (tensor->memoryLocation == MemoryLocation::SpatialArgOut)
      ret << "1, " << varname;
    else
      ret << "1, " << varname << "_dram";
  } else if (op->property == TensorProperty::Dimension) {
    ret << varname;
  } else {
    if (tensor->memoryLocation == MemoryLocation::SpatialArgOut)
      ret << "1, " << varname;
    else
      ret << varname << "_dram";
  }

//  if (property != TensorProperty::Dimension)
//    ret << ", gold_" << varname << "_dram";

  if (!last) 
    ret << ", ";
  else
    ret << ", args";
  return ret.str();
}

// helper to print output store
string CodeGen_Spatial::printOutputStore(map<Expr, string, ExprCompare> varMap,
                                    vector<Expr> inputs, vector<Expr> outputs) {
  stringstream ret;
  unordered_set<string> propsAlreadyGenerated;

  vector<const GetProperty*> sortedProps;
  vector<Expr> props;

  // Add properties to sortedProps and keep track of dimension names
  vector<string> dimPropNames;
  for (auto const& p: varMap) {
    if (p.first.as<GetProperty>()) {
      auto getProperty = p.first.as<GetProperty>();
      sortedProps.push_back(p.first.as<GetProperty>());
      if (getProperty->property == TensorProperty::Dimension)
        dimPropNames.push_back(getProperty->name);
    }
  }

  // sort the properties in order to generate them in a canonical order
  sort(sortedProps.begin(), sortedProps.end(),
       [&](const GetProperty *a,
           const GetProperty *b) -> bool {
         // first, use a total order of outputs,inputs
         auto a_it = find(outputs.begin(), outputs.end(), a->tensor);
         auto b_it = find(outputs.begin(), outputs.end(), b->tensor);
         auto a_pos = distance(outputs.begin(), a_it);
         auto b_pos = distance(outputs.begin(), b_it);
         if (a_it == outputs.end())
           a_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  a->tensor));
         if (b_it == outputs.end())
           b_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  b->tensor));

         // if total order is same, have to do more, otherwise we know
         // our answer
         if (a_pos != b_pos)
           return a_pos < b_pos;

         // if they're different properties, sort by property
         if (a->property != b->property)
           return a->property < b->property;

         // now either the mode gives order, or index #
         if (a->mode != b->mode)
           return a->mode < b->mode;

         return a->index < b->index;
       });

  // Add dimensions for position arrays which are not used by the lowerer
  // This is needed for hardware metadata
  string prevGpName = "";
  for (auto const& getProperty: sortedProps) {
    if (getProperty->property == TensorProperty::Indices && getProperty->index == 0) {
      const Expr tensor = getProperty->tensor;
      const Expr dimensionProperty = ir::GetProperty::make(tensor,
                                                           TensorProperty::Dimension, getProperty->mode);

      if (prevGpName == dimensionProperty.as<GetProperty>()->name) {
        const Expr cscDimProperty = ir::GetProperty::make(tensor,
                                                          TensorProperty::Dimension, getProperty->mode - 1);
        if (std::find(dimPropNames.begin(), dimPropNames.end(), cscDimProperty.as<GetProperty>()->name) == dimPropNames.end())
          props.push_back(cscDimProperty);
      } else if (std::find(dimPropNames.begin(), dimPropNames.end(), dimensionProperty.as<GetProperty>()->name) == dimPropNames.end()){
        props.push_back(dimensionProperty);
      }
    }
    prevGpName = getProperty->name;
  }

  for (auto const& prop: props) {
    if (varMap.count(prop) == 0) {
      auto unique_name = genUniqueName(prop.as<GetProperty>()->name);
      varMap[prop] = unique_name;
    }
    sortedProps.push_back(prop.as<GetProperty>());
  }

  // sort the properties in order to generate them in a canonical order
  sort(sortedProps.begin(), sortedProps.end(),
       [&](const GetProperty *a,
           const GetProperty *b) -> bool {
         // first, use a total order of outputs,inputs
         auto a_it = find(outputs.begin(), outputs.end(), a->tensor);
         auto b_it = find(outputs.begin(), outputs.end(), b->tensor);
         auto a_pos = distance(outputs.begin(), a_it);
         auto b_pos = distance(outputs.begin(), b_it);
         if (a_it == outputs.end())
           a_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  a->tensor));
         if (b_it == outputs.end())
           b_pos += distance(inputs.begin(), find(inputs.begin(), inputs.end(),
                                                  b->tensor));

         // if total order is same, have to do more, otherwise we know
         // our answer
         if (a_pos != b_pos)
           return a_pos < b_pos;

         // if they're different properties, sort by property
         if (a->property != b->property)
           return a->property < b->property;

         // now either the mode gives order, or index #
         if (a->mode != b->mode)
           return a->mode < b->mode;

         return a->index < b->index;
       });

  // Output initMem(...) function for Spatial App
  for (int i = 0; i < (int)sortedProps.size(); i++) {
    auto prop = sortedProps[i];
    bool isOutputProp = (find(outputs.begin(), outputs.end(),
                              prop->tensor) != outputs.end());

    auto var = prop->tensor.as<Var>();
    if (!var->is_parameter && isOutputProp) {
      ret << outputTensorProperty(varMap[prop], prop, isOutputProp);
    }
    propsAlreadyGenerated.insert(varMap[prop]);
  }
  return ret.str();
}

string CodeGen_Spatial::outputTensorProperty(string varname, const GetProperty* op,
                                             bool is_output_prop) {
  stringstream ret;
  string indentation = "    ";

  if (!should_output_store()) {
    return "";
  }

  auto tensor = op->tensor.as<Var>();
  string dims = "";
  for (int i = 1; i < op->index + 1; i++) {
    dims += tensor->name + to_string(i) + "_dimension";
    if (i < op->index) {
      dims += "*";
    }
  }
  if (op->dim.defined())
    dims = op->dim.as<Var>()->name;

  if (op->property == TensorProperty::Values) {
    switch(tensor->memoryLocation) {
      case MemoryLocation::SpatialFIFORetimed:
      case MemoryLocation::SpatialFIFO:
        ret << indentation << varname << "_dram stream_store_vec(0, " << varname << ", " << dims <<  ")" << endl;
        break;
      case MemoryLocation::SpatialSRAM:
        ret << indentation << varname << "_dram(0::" << dims << " par ip) store " << varname << endl;
        break;
      default:
        break;
    }
    return ret.str();
  } else if (op->property == TensorProperty::Dimension) {
    return "";
  } 
  return ret.str();
}

// helper to translate from taco type to C type
string CodeGen_Spatial::printSpatialType(Datatype type, bool is_ptr) {
  stringstream typeStr;
  switch (type.getKind()) {
    case Datatype::Bool: typeStr << "Bool"; break;
    case Datatype::UInt8: typeStr << "U8"; break;
    case Datatype::UInt16: typeStr << "U16"; break;
    case Datatype::UInt32: typeStr << "U32"; break;
    case Datatype::UInt64: typeStr << "U64"; break;
    case Datatype::UInt128: typeStr << "U28"; break;
    case Datatype::Int8: typeStr << "I8"; break;
    case Datatype::Int16: typeStr << "I16"; break;
    case Datatype::Int32: typeStr << "I32"; break;
    case Datatype::Int64: typeStr << "I64"; break;
    case Datatype::Int128: typeStr << "I28"; break;
    case Datatype::Float32:
    case Datatype::Float64: typeStr << "Flt"; break;
    case Datatype::Complex64:
    case Datatype::Complex128:
    case Datatype::Undefined: typeStr << "Undefined"; break;
  }
  stringstream ret;
  ret << typeStr.str();

  return ret.str();
}


} // namespace ir
} // namespace taco