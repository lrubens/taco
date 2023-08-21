//
// Created by oliviahsu on 4/14/22.
//

#ifndef TACO_SAM_PRINTER_H
#define TACO_SAM_PRINTER_H

#include "ops.pb.h"
#include "sam_visitor.h"
#include "stream.pb.h"
#include "tortilla.pb.h"
#include <algorithm>
#include <fcntl.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <map>
#include <ostream>
#include <vector>

namespace taco {
namespace sam {
class SAMPrinter : public SAMVisitor {
public:
  explicit SAMPrinter(std::ostream &os) : os(os) {}

  void print(const SamIR &expr);

  using SAMVisitorStrict::visit;

  void visit(const RootNode *);

  void visit(const FiberLookupNode *) override;

  //        void visit(const FiberWriteNode *);
  //
  //        void visit(const RepeatNode *);
  //
  //        void visit(const RepeatSigGenNode *);
  //
  //        void visit(const IntersectNode *);
  //
  //        void visit(const UnionNode *);
  //
  //        void visit(const ArrayNode *);
  //
  //        void visit(const AddNode *);
  //
  //        void visit(const MulNode *);
  //
  //        void visit(const ReduceNode *);
  //
  //        void visit(const SparseAccumulatorNode *);

  void visit(const CrdDropNode *);
  void visit(const CrdHoldNode *);

private:
  std::ostream &os;
  // ProgramGraph pg;
};

class SAMDotNodePrinter : public SAMVisitor {
public:
  explicit SAMDotNodePrinter(std::ostream &os) : os(os) {}

  void print(const SamIR &sam);

  using SAMVisitor::visit;

  void visit(const RootNode *) override;

  void visit(const BroadcastNode *) override;

  void visit(const FiberLookupNode *) override;

  void visit(const FiberWriteNode *) override;

  void visit(const RepeatNode *) override;

  void visit(const RepeatSigGenNode *) override;

  void visit(const JoinerNode *) override;

  void visit(const ArrayNode *);

  void visit(const ComputeNode *) override;
  void visit(const AddNode *) override;

  void visit(const SparseAccumulatorNode *) override;

  void visit(const CrdDropNode *) override;

  void visit(const CrdHoldNode *) override;

  void setPrintAttributes(bool printAttributes);

private:
  std::ostream &os;
  bool prettyPrint = true;
  bool printAttributes = true;
  std::string tab = "    ";
  std::string name = "SAM";
  std::vector<int> printedNodes;

  std::string printTensorFormats(const RootNode *op);
};

struct Stream {
  ValStream *vstream;
  CrdStream *cstream;
  RefStream *rstream;
  RepSigStream *rsigstream;
  std::string type;
};

class ChannelTrack {
public:
  ChannelTrack(){};
  int get_create_channel(std::string label, int id) {
    auto key = std::make_pair(label, id);
    auto ref = chan_map.find(key);
    if (ref != chan_map.end()) {
      return ref->second;
    }
    auto new_ctr = counter++;
    chan_map[key] = new_ctr;
    return new_ctr;
  }

  int create_channel(std::string label, int id) {
    auto key = std::make_pair(label, id);
    auto new_ctr = counter++;
    chan_map[key] = new_ctr;
    return new_ctr;
  }

private:
  std::map<std::pair<std::string, int>, int> chan_map;
  int counter = 1;
};

class SAMDotEdgePrinter : public SAMVisitor {
public:
  explicit SAMDotEdgePrinter(std::ostream &os) : os(os) {}

  void print(const SamIR &sam);

  using SAMVisitor::visit;

  void visit(const RootNode *) override;

  void visit(const BroadcastNode *) override;

  void visit(const FiberLookupNode *) override;

  void visit(const FiberWriteNode *) override;

  void visit(const RepeatNode *) override;

  void visit(const RepeatSigGenNode *) override;

  void visit(const JoinerNode *) override;

  void visit(const ArrayNode *) override;

  void visit(const ComputeNode *) override;

  void visit(const CrdDropNode *) override;

  void visit(const SparseAccumulatorNode *) override;

  void visit(const CrdHoldNode *) override;

  void setPrintAttributes(bool printAttributes);

private:
  std::string printerHelper();

  std::ostream &os;
  bool prettyPrint = true;
  bool printAttributes = true;
  std::string tab = "    ";
  std::string edgeType;
  std::string edgeIndex;
  std::vector<int> printedNodes;
  std::map<std::string, std::string> edgeStyle = {{"ref", " style=bold"},
                                                  {"crd", " style=dashed"},
                                                  {"repsig", " style=dotted"},
                                                  {"bv", " style=dashed"},
                                                  {"", ""}};
  bool printComment = false;
  std::string comment;
  std::string label;

  std::string curr_comment;

  int full_joiner;
  int val_writer_id = -1;
  Operation *curr_op;
  Stream out_stream;
  int channel = 0;
  ChannelTrack chan_track;
  std::map<std::pair<std::string, int>, int> chan_map;
};

} // namespace sam
} // namespace taco

#endif // TACO_SAM_PRINTER_H
