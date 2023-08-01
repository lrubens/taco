//
// Created by oliviahsu on 4/14/22.
//

#include <algorithm>
#include "sam_printer.h"
#include "sam_nodes.h"
#include "taco/util/collections.h"
#include "taco/util/strings.h"

using namespace std;

namespace taco
{
    namespace sam
    {
        std::map<int, Operation *> id_to_op;
        ProgramGraph pg;
        // SAMPrinter
        void SAMPrinter::visit(const FiberLookupNode *op)
        {
            // os << op->getName();
            // os << "->" << endl;
            op->out_crd.accept(this);
        }

        void SAMPrinter::visit(const RootNode *op)
        {
            // os << op->getName();
            // os << "->" << endl;
            for (auto node : op->nodes)
                node.accept(this);
        }

        void SAMPrinter::visit(const CrdDropNode *op)
        {
            // os << op->getName();
            // os << "->" << endl;

            if (op->out_outer_crd.defined())
                op->out_outer_crd.accept(this);
            if (op->out_inner_crd.defined())
                op->out_inner_crd.accept(this);
        }

        void SAMPrinter::visit(const CrdHoldNode *op)
        {
            // os << op->getName();
            // os << "->" << endl;

            if (op->out_outer_crd.defined())
                op->out_outer_crd.accept(this);
            if (op->out_inner_crd.defined())
                op->out_inner_crd.accept(this);
        }

        void SAMPrinter::print(const SamIR &sam)
        {
            sam.accept(this);
        }

        string SAMDotNodePrinter::printTensorFormats(const RootNode *op)
        {
            stringstream ss;
            for (int i = 0; i < (int)op->tensors.size(); i++)
            {
                auto t = op->tensors.at(i);
                vector<char> formats;
                for (auto f : t.getFormat().getModeFormats())
                {
                    if (f == ModeFormat::Dense)
                        formats.push_back('d');
                    else if (f == ModeFormat::Sparse)
                        formats.push_back('s');
                    else if (f == ModeFormat::Sparse(ModeFormat::NOT_UNIQUE))
                        formats.push_back('u');
                    else if (f == ModeFormat::Sparse(ModeFormat::ZEROLESS))
                        formats.push_back('z');
                    else if (f == ModeFormat::Singleton(ModeFormat::NOT_UNIQUE))
                        formats.push_back('c');
                    else if (f == ModeFormat::Singleton)
                        formats.push_back('q');
                    else
                        taco_iassert(false) << "Format not supported" << endl;
                }
                ss << t.getName() << "=" << (formats.empty() ? "none" : util::join(formats, "")) << util::join(t.getFormat().getModeOrdering(), "");
                if (i != (int)op->tensors.size() - 1)
                {
                    ss << ",";
                }
                else
                {
                    ss << "\"";
                }
            }
            return ss.str();
        }

        void SAMDotNodePrinter::print(const SamIR &sam)
        {
            // os << "digraph " << name << " {" << endl;
            sam.accept(this);
        }

        // SAMDotNodePrinter
        void SAMDotNodePrinter::visit(const RootNode *op)
        {
            // Add comments so formats are passed along
            pg.set_name(printTensorFormats(op));
            // os << tab;
            // os << "comment=\"" << printTensorFormats(op) << endl;
            for (const auto &node : op->nodes)
            {
                node.accept(this);
            }
        }

        // SAMDotNodePrinter
        void SAMDotNodePrinter::visit(const BroadcastNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {

                Operation *new_op = pg.add_operators();
                new_op->set_id(op->nodeID + 1);
                new_op->set_name("broadcast");
                new_op->mutable_broadcast()->set_label("broadcast");
                id_to_op[op->nodeID + 1] = new_op;

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=\"type=broadcast\"";
                // if (prettyPrint) {
                //     os << " shape=point style=invis ";
                // }
                // if (printAttributes) {
                //     os << "type=\"broadcast\"";
                // }
                // os << "]" << endl;

                for (const auto &node : op->outputs)
                {
                    if (node.defined())
                    {
                        node.accept(this);
                    }
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const FiberLookupNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                string src = op->source ? ",src=true" : ",src=false";
                string root = op->root ? ",root=true" : ",root=false";

                // std::stringstream comment;
                // comment << "\"type=fiberlookup,index=" << op->i.getName() << ",tensor=" << op->tensorVar.getName()
                //         << ",mode=" << std::to_string(op->mode)
                //         << ",format=" << op->modeFormat.getName() << src << root << "\"";

                Operation *new_op = pg.add_operators();
                new_op->set_id(op->nodeID + 1);
                new_op->set_name("fiberlookup");
                new_op->mutable_fiber_lookup()->set_root(op->root);
                new_op->mutable_fiber_lookup()->set_format(op->modeFormat.getName());
                new_op->mutable_fiber_lookup()->set_src(op->source);
                new_op->mutable_fiber_lookup()->set_tensor(op->tensorVar.getName());
                new_op->mutable_fiber_lookup()->set_mode(op->mode);
                new_op->mutable_fiber_lookup()->set_index(op->i.getName());
                new_op->mutable_fiber_lookup()->set_label(op->getName());
                id_to_op[op->nodeID + 1] = new_op;

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\"";
                //     os << " color=green4 shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"fiberlookup\""
                //           " index=\"" << op->i.getName() << "\"" <<
                //           " tensor=\"" << op->tensorVar.getName() << "\"" <<
                //           " mode=\"" << std::to_string(op->mode) << "\"" <<
                //           " format=\"" << op->modeFormat.getName() << "\"" <<
                //           " src=\"" << (op->source ? "true" : "false") << "\"" <<
                //           " root=\"" << (op->root ? "true" : "false") << "\"";
                // }
                // os << "]" << endl;

                if (op->out_crd.defined())
                {
                    op->out_crd.accept(this);
                }

                if (op->out_ref.defined())
                {
                    op->out_ref.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const FiberWriteNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                string sink = op->sink ? ",sink=true" : ",sink=false";

                std::stringstream comment;

                Operation *new_op = pg.add_operators();
                new_op->set_id(op->nodeID + 1);

                if (op->vals)
                {

                    new_op->set_name("valwrite");
                    new_op->mutable_val_write()->set_tensor(op->tensorVar.getName());
                    new_op->mutable_val_write()->set_sink(op->sink);
                    new_op->mutable_val_write()->set_crdsize(op->maxCrdSize);
                    new_op->mutable_val_write()->set_label(op->getName());
                    // Hardcoding the valwrite values since it doesn't have a nodeID
                    id_to_op[op->nodeID + 1] = new_op;
                }
                else
                {
                    // comment << "\"type=fiberwrite,index=" << op->i.getName() << ",tensor=" << op->tensorVar.getName()
                    //         << ",mode=" << std::to_string(op->mode)
                    //         << ",format=" << op->modeFormat.getName();
                    if (op->modeFormat == compressed)
                    {
                        // comment << ",segsize=" << op->maxSegSize
                        //         << ",crdsize=" << op->maxCrdSize;
                        new_op->mutable_fiber_write()->set_crdsize(op->maxCrdSize);
                        new_op->mutable_fiber_write()->set_segsize(op->maxSegSize);
                    }
                    // comment << sink << "\"";
                    new_op->set_name("fiberwrite");
                    // new_op->set_id(op->nodeID + 1);
                    new_op->mutable_fiber_write()->set_tensor(op->tensorVar.getName());
                    new_op->mutable_fiber_write()->set_index(op->i.getName());
                    new_op->mutable_fiber_write()->set_sink(op->sink);
                    new_op->mutable_fiber_write()->set_label(op->getName());
                    new_op->mutable_fiber_write()->set_format(op->modeFormat.getName());
                    id_to_op[op->nodeID + 1] = new_op;
                }

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\"";
                //     os << " color=green3 shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"fiberwrite\"";
                //     if (op->vals) {
                //         os << " tensor=\"" << op->tensorVar.getName() << "\"" <<
                //               " mode=\"vals\"" <<
                //               " size=\"" << op->maxCrdSize << "\"";
                //     } else {
                //         os << " index=\"" << op->i.getName() << "\"" <<
                //               " tensor=\"" << op->tensorVar.getName() << "\"" <<
                //               " mode=\"" << std::to_string(op->mode) << "\"" <<
                //               " format=\"" << op->modeFormat.getName() << "\"";
                //         if (op->modeFormat == compressed) {
                //             os << " segsize=\"" << op->maxSegSize << "\"" <<
                //                   " crdsize=\"" << op->maxCrdSize << "\"";
                //         }
                //     }
                //     os << " sink=" << (op->sink ? "\"true\"" : "\"false\"");

                // }
                // os << "]" << endl;
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const RepeatNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                string root = op->root ? ",root=true" : ",root=false";

                Operation *new_op = pg.add_operators();
                new_op->set_name("repeat");
                new_op->set_id(op->nodeID + 1);
                new_op->mutable_repeat()->set_label(op->getName());
                new_op->mutable_repeat()->set_index(op->i.getName());
                new_op->mutable_repeat()->set_root(op->root);
                new_op->mutable_repeat()->set_tensor(op->tensorVar.getName());
                id_to_op[op->nodeID + 1] = new_op;

                std::stringstream comment;
                // comment << "\"type=repeat,index=" << op->i.getName() << ",tensor=" << op->tensorVar.getName() << root << "\"";

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\"";
                //     os << " color=cyan2 shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"repeat\""
                //           " index=\"" << op->i.getName() << "\"" <<
                //           " tensor=\"" << op->tensorVar.getName() << "\"" <<
                //           " root=\"" << (op->root ? "true" : "false") << "\"";
                // }
                // os << "]" << endl;

                if (op->out_ref.defined())
                {
                    op->out_ref.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const RepeatSigGenNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {

                std::stringstream comment;
                // comment << "\"type=repsiggen,index=" << op->i.getName() << "\"";

                Operation *new_op = pg.add_operators();
                new_op->set_name("repsiggen");
                new_op->set_id(op->nodeID + 1);
                new_op->mutable_repeatsig()->set_label(op->getName());
                new_op->mutable_repeatsig()->set_index(op->i.getName());
                id_to_op[op->nodeID + 1] = new_op;

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\"";
                //     os << " color=cyan3 shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"repsiggen\""
                //           " index=\"" << op->i.getName() << "\"";
                // }
                // os << "]" << endl;

                if (op->out_repsig.defined())
                {
                    op->out_repsig.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const JoinerNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                std::stringstream comment;
                // comment << "\"type=" << op->getNodeName() << ",index=" << op->i.getName() << "\"";

                Operation *new_op = pg.add_operators();
                new_op->set_name(op->getNodeName());
                new_op->set_id(op->nodeID + 1);
                if (op->getNodeName() == "intersect")
                {
                    new_op->mutable_joiner()->set_join_type(Joiner_Type_INTERSECT);
                }
                else
                {
                    new_op->mutable_joiner()->set_join_type(Joiner_Type_UNION);
                }
                new_op->mutable_joiner()->set_label(op->getName());
                new_op->mutable_joiner()->set_index(op->i.getName());
                id_to_op[op->nodeID + 1] = new_op;

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\"";
                //     os << " color=purple shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"" << op->getNodeName() << "\""
                //           " index=\"" << op->i.getName() << "\"";
                // }
                // os << "]" << endl;

                if (op->out_crd.defined())
                {
                    op->out_crd.accept(this);
                }

                for (auto out_ref : op->out_refs)
                {
                    if (out_ref.defined())
                        out_ref.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const ArrayNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                std::stringstream comment;
                // comment << "\"type=arrayvals,tensor=" << op->tensorVar.getName() << "\"";

                Operation *new_op = pg.add_operators();
                new_op->set_name("arrayvals");
                new_op->set_id(op->nodeID + 1);
                new_op->mutable_array()->set_tensor("arrayvals");
                new_op->mutable_array()->set_label(op->getName());
                id_to_op[op->nodeID + 1] = new_op;
                // new_op->mutable_array()->mutable_in_ref()->set_id(op->nodeID + 1);
                // new_op->mutable_array()->mutable_out_val()->set_id(op->nodeID + 1);

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\"";
                //     os << " color=green2 shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"arrayvals\""
                //           " tensor=\"" << op->tensorVar.getName() << "\"";
                // }
                // os << "]" << endl;

                if (op->out_val.defined())
                {
                    op->out_val.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const ComputeNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                std::stringstream comment;
                // comment << "\"type=" << op->getNodeName() <<"\"";

                Operation *new_op = pg.add_operators();
                new_op->set_name(op->getNodeName());
                new_op->set_id(op->nodeID + 1);
                if (op->getNodeName() != "reduce")
                {
                    ALU_Stage *new_stage = new_op->mutable_alu()->add_stages();
                    if (op->getNodeName() == "mul")
                    {
                        new_stage->set_op(ALU_ALUOp_MUL);
                    }
                    else if (op->getNodeName() == "div")
                    {
                        new_stage->set_op(ALU_ALUOp_DIV);
                    }
                    new_stage->add_inputs(0);
                    new_stage->add_inputs(1);
                    new_stage->set_output(0);
                    new_op->mutable_alu()->set_output_val(0);
                    new_op->mutable_alu()->set_label(op->getName());
                }
                // else {
                //     cout << op->getNodeName() << endl;
                //     exit(0);
                // }
                id_to_op[op->nodeID + 1] = new_op;

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\"";
                //     os << " color=brown shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"" << op->getNodeName() << "\"";
                // }
                // os << "]" << endl;

                if (op->out_val.defined())
                {
                    op->out_val.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const AddNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                std::stringstream comment;
                // comment << "\"type=" << op->getNodeName() <<",sub=" << op->sub <<"\"";

                Operation *new_op = pg.add_operators();
                new_op->set_name(op->getNodeName());
                new_op->set_id(op->nodeID + 1);
                ALU_Stage *new_stage = new_op->mutable_alu()->add_stages();
                if (op->sub)
                {
                    new_stage->set_op(ALU_ALUOp_SUB);
                }
                else
                {
                    new_stage->set_op(ALU_ALUOp_ADD);
                }
                new_stage->add_inputs(0);
                new_stage->add_inputs(1);
                new_stage->set_output(0);
                new_op->mutable_alu()->set_output_val(0);
                new_op->mutable_alu()->set_label(op->getName());
                id_to_op[op->nodeID + 1] = new_op;

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName();
                //     if (op->sub)
                //         os << "\nsubtract\"";
                //     else
                //         os << "\"";
                //     os << " color=brown shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"" << op->getNodeName() << "\"";
                //     os << " sub=\"" << op->sub << "\"";
                // }
                // os << "]" << endl;

                if (op->out_val.defined())
                {
                    op->out_val.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const SparseAccumulatorNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                std::stringstream comment;
                // comment << "\"type=" << op->getNodeName() <<",order=" << op->order;

                Operation *new_op = pg.add_operators();
                new_op->set_name(op->getNodeName());
                new_op->set_id(op->nodeID + 1);

                for (auto ivar : op->ivarMap)
                {
                    // comment << ",in" << to_string(ivar.first) << "=" << ivar.second.getName();
                    if (to_string(ivar.first) == "0")
                    {
                        new_op->mutable_spacc()->set_inner_crd(ivar.second.getName());
                    }
                    else
                    {
                        new_op->mutable_spacc()->add_outer_crd(ivar.second.getName());
                    }
                }
                // comment << "\"";

                new_op->mutable_spacc()->set_label(op->getName());
                new_op->mutable_spacc()->set_order(op->order);
                id_to_op[op->nodeID + 1] = new_op;

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\n";
                //     for (auto ivar : op->ivarMap) {
                //         os << to_string(ivar.first) << "=" << ivar.second.getName() << " ";
                //     }
                //     os << "\"";
                //     os << " color=brown shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os << " type=\"" << op->getNodeName() << "\"" <<
                //           " order=\"" << op->order << "\"";
                //     for (auto ivar : op->ivarMap) {
                //         os << " in" << to_string(ivar.first) << "=\"" << ivar.second.getName() << "\"";
                //     }
                // }
                // os << "]" << endl;

                if (op->out_val.defined())
                {
                    op->out_val.accept(this);
                }

                for (auto out_crd : op->out_crds)
                {
                    out_crd.second.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const CrdDropNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                std::stringstream comment;
                // comment << "\"type=crddrop,outer=" << op->outer << ",inner=" << op->inner << "\"";

                Operation *new_op = pg.add_operators();
                new_op->set_name("crddrop");
                new_op->set_id(op->nodeID + 1);
                new_op->mutable_coord_drop()->set_inner_crd(op->inner.getName());
                new_op->mutable_coord_drop()->set_outer_crd(op->outer.getName());
                new_op->mutable_coord_drop()->set_label(op->getName());
                id_to_op[op->nodeID + 1] = new_op;
                // new_op->mutable_array()->set_label(op->getName());

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\"";
                //     os << " color=orange shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os <<   " type=\"" << "crddrop" << "\"" <<
                //             " outer=\"" << op->outer.getName() << "\"" <<
                //             " inner=\"" << op->inner.getName() << "\"";

                // }
                // os << "]" << endl;

                if (op->out_outer_crd.defined())
                {
                    op->out_outer_crd.accept(this);
                }

                if (op->out_inner_crd.defined())
                {
                    op->out_inner_crd.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::visit(const CrdHoldNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                std::stringstream comment;
                // comment << "\"type=crdhold,outer=" << op->outer << ",inner=" << op->inner << "\"";

                Operation *new_op = pg.add_operators();
                new_op->set_name("crdhold");
                new_op->set_id(op->nodeID + 1);
                new_op->mutable_coord_hold()->set_inner_crd(op->inner.getName());
                new_op->mutable_coord_hold()->set_outer_crd(op->outer.getName());
                new_op->mutable_coord_hold()->set_label(op->getName());

                id_to_op[op->nodeID + 1] = new_op;

                // os << tab;
                // os << to_string(op->nodeID) << " [comment=" << comment.str();
                // if (prettyPrint) {
                //     os << " label=\"" << op->getName() << "\nouter="<< op->outer << ",inner=" << op->inner << "\"";
                //     os << " color=orange shape=box style=filled";
                // }
                // if (printAttributes) {
                //     os <<   " type=\"" << "crdhold" << "\"" <<
                //        " outer=\"" << op->outer.getName() << "\"" <<
                //        " inner=\"" << op->inner.getName() << "\"";

                // }
                // os << "]" << endl;

                if (op->out_outer_crd.defined())
                {
                    op->out_outer_crd.accept(this);
                }

                if (op->out_inner_crd.defined())
                {
                    op->out_inner_crd.accept(this);
                }
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotNodePrinter::setPrintAttributes(bool printAttributes)
        {
            this->printAttributes = printAttributes;
        }

        // SAM Dot Edge Printer
        void SAMDotEdgePrinter::print(const SamIR &sam)
        {
            sam.accept(this);
            std::string str;
            google::protobuf::TextFormat::PrintToString(pg, &str);
            // printf("%s", str.c_str());
            os << str << endl;

            // const char *filename = "./test.prototxt";
            // int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            // if (fd == -1)
            //     cout << "File not found: " << filename;

            // google::protobuf::io::FileOutputStream* output = new google::protobuf::io::FileOutputStream(os);
            // // google::protobuf::io::OstreamOutputStream* output = new google::protobuf::io::OstreamOutputStream(os);
            // if (!google::protobuf::TextFormat::Print(pg, output)) {
            //     cerr << "Error writing text proto to file" << endl;
            // }
            // output->Flush();
            // close(fd);
        }

        void SAMDotEdgePrinter::visit(const RootNode *op)
        {
            for (const auto &node : op->nodes)
            {
                // curr_op = id_to_op[op->nodeID + 1];
                node.accept(this);
            }
            // os << "}" << endl;
        }

        void SAMDotEdgePrinter::visit(const BroadcastNode *op)
        {
            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                string ss = printerHelper();
                // os << op->nodeID << ss << endl;

                if (out_stream.type == "crd")
                {
                    out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "ref")
                {
                    out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "repsig")
                {
                    out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "val")
                {
                    out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
                }

                if (op->type == SamEdgeType::crd)
                {
                    id_to_op[op->nodeID + 1]->mutable_broadcast()->mutable_input()->set_name(label);
                    if (curr_op)
                    {
                        id_to_op[op->nodeID + 1]->mutable_broadcast()->mutable_input()->mutable_id()->set_id(curr_op->id());
                        // cout << id_to_op[op->nodeID + 1]->name() << curr_op->id() << endl;
                        // cout << id_to_op[op->nodeID + 1]->mutable_broadcast()->mutable_input()->name() << endl;
                    }
                }

                for (SamIR node : op->outputs)
                {
                    if (node.defined())
                    {
                        if (op->printEdgeName)
                        {
                            printComment = op->printEdgeName;
                        }
                        comment = contains(op->edgeName, node) ? op->edgeName.at(node) : "";
                        switch (op->type)
                        {
                        case SamEdgeType::crd:
                            edgeType = "crd";
                            break;
                        case SamEdgeType::ref:
                            edgeType = "ref";
                            break;
                        case SamEdgeType::repsig:
                            edgeType = "repsig";
                            break;
                        case SamEdgeType::val:
                        default:
                            edgeType = "";
                            break;
                        }
                        // os << tab << op->nodeID << " -> ";
                        curr_op = id_to_op[op->nodeID + 1];
                        out_stream.cstream = curr_op->mutable_broadcast()->add_outputs();
                        out_stream.cstream->set_name(comment);
                        out_stream.type = edgeType;
                        node.accept(this);
                    }
                }
                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const FiberLookupNode *op)
        {
            if (!op->root)
            {
                string ss = printerHelper();
                // os << op->nodeID << ss << endl;
                if (out_stream.type == "crd")
                {
                    out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "ref")
                {
                    out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "repsig")
                {
                    out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "val")
                {
                    out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
                }
                id_to_op[op->nodeID + 1]->mutable_fiber_lookup()->mutable_input_ref()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_fiber_lookup()->mutable_input_ref()->mutable_id()->set_id(curr_op->id());
                }
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {

                // os << "INSIDE COUNT FOUND" << endl;
                if (op->out_crd.defined())
                {
                    printComment = op->printEdgeName;
                    comment = contains(op->edgeName, op->out_crd) ? op->edgeName.at(op->out_crd) : "";
                    edgeType = "crd";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    out_stream.cstream = curr_op->mutable_fiber_lookup()->mutable_output_crd();
                    out_stream.cstream->set_name(comment);
                    out_stream.type = edgeType;
                    op->out_crd.accept(this);
                }

                if (op->out_ref.defined())
                {
                    printComment = op->printEdgeName;
                    comment = contains(op->edgeName, op->out_ref) ? op->edgeName.at(op->out_ref) : "";
                    edgeType = "ref";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    out_stream.rstream = curr_op->mutable_fiber_lookup()->mutable_output_ref();
                    out_stream.rstream->set_name(comment);
                    out_stream.type = edgeType;
                    op->out_ref.accept(this);
                }

                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const FiberWriteNode *op)
        {
            string ss = printerHelper();
            // os << op->nodeID << ss << endl;


            if (out_stream.type == "crd")
            {
                out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "ref")
            {
                out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "repsig")
            {
                out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "val")
            {
                // if (curr_op->name() == "reduce")
                    // cout << "REDUCE FOUND: " << out_stream.vstream->name() << ": " << op->nodeID << endl;
                // exit(0);
                out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
            }

            if (!op->vals)
            {
                id_to_op[op->nodeID + 1]->mutable_fiber_write()->mutable_input_crd()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_fiber_write()->mutable_input_crd()->mutable_id()->set_id(curr_op->id());
                }
            }
            else
            {
                id_to_op[op->nodeID + 1]->mutable_val_write()->mutable_input_val()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_val_write()->mutable_input_val()->mutable_id()->set_id(curr_op->id());
                }
            }

            edgeType = "";

            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const RepeatNode *op)
        {
            string ss = printerHelper();
            // os << op->nodeID << ss << endl;

            if (out_stream.type == "crd")
            {
                out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "ref")
            {
                out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "repsig")
            {
                out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "val")
            {
                out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
            }

            if (curr_op->name() == "repsiggen")
            {
                id_to_op[op->nodeID + 1]->mutable_repeat()->mutable_input_rep_sig()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_repeat()->mutable_input_rep_sig()->mutable_id()->set_id(curr_op->id());
                }
            }
            else
            {
                id_to_op[op->nodeID + 1]->mutable_repeat()->mutable_input_ref()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_repeat()->mutable_input_ref()->mutable_id()->set_id(curr_op->id());
                }
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                if (op->out_ref.defined())
                {
                    edgeType = "ref";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    out_stream.rstream = curr_op->mutable_repeat()->mutable_output_ref();
                    out_stream.rstream->set_name(edgeType);
                    out_stream.type = edgeType;
                    op->out_ref.accept(this);
                }
                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const RepeatSigGenNode *op)
        {
            string ss = printerHelper();
            // os << op->nodeID << ss << endl;

            if (out_stream.type == "crd")
            {
                out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "ref")
            {
                out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "repsig")
            {
                out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "val")
            {
                out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
            }

            id_to_op[op->nodeID + 1]->mutable_repeatsig()->mutable_input_crd()->set_name(label);
            if (curr_op)
            {
                id_to_op[op->nodeID + 1]->mutable_repeatsig()->mutable_input_crd()->mutable_id()->set_id(curr_op->id());
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                if (op->out_repsig.defined())
                {
                    edgeType = "repsig";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    out_stream.rsigstream = curr_op->mutable_repeatsig()->mutable_output_rep_sig();
                    out_stream.rsigstream->set_name(edgeType);
                    out_stream.type = edgeType;
                    op->out_repsig.accept(this);
                }
                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const JoinerNode *op)
        {
            string ss = printerHelper();
            // os << op->nodeID << ss << endl;

            if (out_stream.type == "crd")
            {
                out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "ref")
            {
                out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "repsig")
            {
                out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "val")
            {
                out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
            }

            if (edgeType == "ref")
            {
                int num_pairs = id_to_op[op->nodeID + 1]->mutable_joiner()->input_pairs_size();
                Joiner::JoinBundle *bundle;
                if (num_pairs == 0 || full_joiner == 2)
                {
                    bundle = id_to_op[op->nodeID + 1]->mutable_joiner()->add_input_pairs();
                    full_joiner = 0;
                    num_pairs++;
                }
                bundle = id_to_op[op->nodeID + 1]->mutable_joiner()->mutable_input_pairs(num_pairs - 1);
                if (curr_op)
                {
                    bundle->mutable_ref()->mutable_id()->set_id(curr_op->id());
                    bundle->mutable_ref()->set_name(label);
                    full_joiner += 1;
                }
            }
            else
            {
                int num_pairs = id_to_op[op->nodeID + 1]->mutable_joiner()->input_pairs_size();
                Joiner::JoinBundle *bundle;
                if (num_pairs == 0 || full_joiner == 2)
                {
                    bundle = id_to_op[op->nodeID + 1]->mutable_joiner()->add_input_pairs();
                    full_joiner = 0;
                    num_pairs++;
                }
                bundle = id_to_op[op->nodeID + 1]->mutable_joiner()->mutable_input_pairs(num_pairs - 1);
                if (curr_op)
                {
                    bundle->mutable_crd()->mutable_id()->set_id(curr_op->id());
                    bundle->mutable_crd()->set_name(label);
                    full_joiner += 1;
                }
            }

            if (edgeType == "ref")
            {
                auto op_addr = (JoinerNode **)&op;
                (*op_addr)->numInputs += 1;
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                if (op->out_crd.defined())
                {
                    printComment = op->printEdgeName;
                    comment = op->edgeName;
                    edgeType = "crd";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    out_stream.cstream = curr_op->mutable_joiner()->mutable_output_crd();
                    out_stream.cstream->set_name(comment);
                    out_stream.type = edgeType;
                    op->out_crd.accept(this);
                }

                for (int i = 0; i < (int)op->out_refs.size(); i++)
                {
                    auto out_ref = op->out_refs.at(i);
                    if (out_ref.defined())
                    {
                        printComment = true;
                        comment = "out-" + out_ref.getTensorName();
                        edgeType = "ref";
                        // os << tab << op->nodeID << " -> ";
                        curr_op = id_to_op[op->nodeID + 1];
                        if (i == 0)
                        {
                            out_stream.rstream = curr_op->mutable_joiner()->mutable_output_ref1();
                            out_stream.rstream->set_name(comment);
                        }
                        else
                        {
                            out_stream.rstream = curr_op->mutable_joiner()->mutable_output_ref2();
                            out_stream.rstream->set_name(comment);
                        }
                        out_stream.type = edgeType;
                        out_ref.accept(this);
                    }
                }
                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const ArrayNode *op)
        {
            if (!op->root)
            {
                string ss = printerHelper();
                // os << op->nodeID << ss << endl;
                if (out_stream.type == "crd")
                {
                    out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "ref")
                {
                    out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "repsig")
                {
                    out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
                }
                else if (out_stream.type == "val")
                {
                    out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
                }
                id_to_op[op->nodeID + 1]->mutable_array()->mutable_input_ref()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_array()->mutable_input_ref()->mutable_id()->set_id(curr_op->id());
                }
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                if (op->out_val.defined())
                {
                    edgeType = "";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    curr_op->mutable_array()->mutable_output_val()->set_name("val");
                    out_stream.vstream = curr_op->mutable_array()->mutable_output_val();
                    out_stream.type = "val";
                    op->out_val.accept(this);
                }

                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const ComputeNode *op)
        {
            string ss = printerHelper();
            // os << op->nodeID << ss << endl;

            if (out_stream.type == "crd")
            {
                out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "ref")
            {
                out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "repsig")
            {
                out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "val")
            {
                if (op->getNodeName() == "reduce") {
                    // cout << "ID for red : " << op->nodeID << endl;
                    // exit(0);
                }
                out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
            }

            if (op->getNodeName() != "reduce")
            {
                ValStream *alu_val = id_to_op[op->nodeID + 1]->mutable_alu()->mutable_vals()->add_inputs();
                alu_val->set_name(label);
                if (curr_op)
                {
                    int num_inputs = id_to_op[op->nodeID + 1]->mutable_alu()->mutable_vals()->inputs_size();
                    id_to_op[op->nodeID + 1]->mutable_alu()->mutable_vals()->mutable_inputs(num_inputs - 1)->mutable_id()->set_id(curr_op->id());
                }
            }
            else
            {
                id_to_op[op->nodeID + 1]->mutable_reduce()->mutable_input_val()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_reduce()->mutable_input_val()->mutable_id()->set_id(curr_op->id());
                }
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                if (op->out_val.defined())
                {
                    edgeType = "";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    if (op->getNodeName() != "reduce") {
                        out_stream.vstream = curr_op->mutable_alu()->mutable_vals()->mutable_output();
                        out_stream.vstream->set_name("val");
                    } else {
                        out_stream.vstream = curr_op->mutable_reduce()->mutable_output_val();
                        out_stream.vstream->set_name("reduce_val");
                    }
                    out_stream.type = "val";
                    op->out_val.accept(this);
                }

                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const SparseAccumulatorNode *op)
        {
            string ss = printerHelper();
            // os << op->nodeID << ss << endl;

            if (out_stream.type == "crd")
            {
                out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "ref")
            {
                out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "repsig")
            {
                out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "val")
            {
                out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
            }

            if (edgeType == "crd")
            {
                // std::cout << string(&label.back()) << endl;
                // exit(0);
                if (string(&label.back()) == id_to_op[op->nodeID + 1]->mutable_spacc()->inner_crd())
                {
                    id_to_op[op->nodeID + 1]->mutable_spacc()->mutable_input_inner_crd()->set_name(label);
                    if (curr_op)
                    {
                        id_to_op[op->nodeID + 1]->mutable_spacc()->mutable_input_inner_crd()->mutable_id()->set_id(curr_op->id());
                    }
                }
                else
                {
                    id_to_op[op->nodeID + 1]->mutable_spacc()->mutable_input_outer_crd()->set_name(label);
                    if (curr_op)
                    {
                        id_to_op[op->nodeID + 1]->mutable_spacc()->mutable_input_outer_crd()->mutable_id()->set_id(curr_op->id());
                    }
                }
            }
            else
            {
                // std::cout << label << endl;
                id_to_op[op->nodeID + 1]->mutable_spacc()->mutable_input_val()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_spacc()->mutable_input_val()->mutable_id()->set_id(curr_op->id());
                }
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                if (op->out_val.defined())
                {
                    edgeType = "";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    curr_op->mutable_spacc()->mutable_output_val()->set_name(comment);
                    out_stream.vstream = curr_op->mutable_spacc()->mutable_output_val();
                    out_stream.type = "val";
                    op->out_val.accept(this);
                }

                for (auto out_crd : op->out_crds)
                {
                    printComment = true;
                    comment = "out-" + op->ivarMap.at(out_crd.first).getName();
                    std::cout << comment << endl;
                    edgeType = "crd";
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    curr_op->mutable_spacc()->mutable_output_inner_crd()->set_name(comment);
                    out_stream.cstream = curr_op->mutable_spacc()->mutable_output_inner_crd();
                    out_stream.type = "crd";
                    out_crd.second.accept(this);
                }

                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const CrdDropNode *op)
        {
            string ss = printerHelper();
            // os << op->nodeID << ss << endl;
            if (out_stream.type == "crd")
            {
                out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "ref")
            {
                out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "repsig")
            {
                out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
            }
            else if (out_stream.type == "val")
            {
                out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
            }

            // std::cout << string(&label.back()) << endl;
            // exit(0);
            if (string(&label.back()) == id_to_op[op->nodeID + 1]->mutable_coord_drop()->inner_crd())
            {
                id_to_op[op->nodeID + 1]->mutable_coord_drop()->mutable_input_inner_crd()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_coord_drop()->mutable_input_inner_crd()->mutable_id()->set_id(curr_op->id());
                }
            }
            else
            {
                id_to_op[op->nodeID + 1]->mutable_coord_drop()->mutable_input_outer_crd()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_coord_drop()->mutable_input_outer_crd()->mutable_id()->set_id(curr_op->id());
                }
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                if (op->out_outer_crd.defined())
                {
                    edgeType = "crd";
                    printComment = true;
                    comment = "outer-" + op->outer.getName();
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    curr_op->mutable_coord_drop()->mutable_output_outer_crd()->set_name(comment);
                    out_stream.cstream = curr_op->mutable_coord_drop()->mutable_output_outer_crd();
                    out_stream.type = "crd";
                    op->out_outer_crd.accept(this);
                }

                if (op->out_inner_crd.defined())
                {
                    edgeType = "crd";
                    printComment = true;
                    comment = "inner-" + op->inner.getName();
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    curr_op->mutable_coord_drop()->mutable_output_inner_crd()->set_name(comment);
                    out_stream.cstream = curr_op->mutable_coord_drop()->mutable_output_inner_crd();
                    out_stream.type = "crd";
                    op->out_inner_crd.accept(this);
                }

                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        void SAMDotEdgePrinter::visit(const CrdHoldNode *op)
        {
            string ss = printerHelper();
            // os << op->nodeID << ss << endl;

            if (out_stream.type == "crd") {
            out_stream.cstream->mutable_id()->set_id(op->nodeID + 1);
        } else if (out_stream.type == "ref") {
            out_stream.rstream->mutable_id()->set_id(op->nodeID + 1);
        } else if (out_stream.type == "repsig") {
            out_stream.rsigstream->mutable_id()->set_id(op->nodeID + 1);
        } else if (out_stream.type == "val") {
            out_stream.vstream->mutable_id()->set_id(op->nodeID + 1);
        }

            if (string(&label.back()) == id_to_op[op->nodeID + 1]->mutable_coord_hold()->inner_crd())
            {
                id_to_op[op->nodeID + 1]->mutable_coord_hold()->mutable_input_inner_crd()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_coord_hold()->mutable_input_inner_crd()->mutable_id()->set_id(curr_op->id());
                }
            }
            else
            {
                id_to_op[op->nodeID + 1]->mutable_coord_hold()->mutable_input_outer_crd()->set_name(label);
                if (curr_op)
                {
                    id_to_op[op->nodeID + 1]->mutable_coord_hold()->mutable_input_outer_crd()->mutable_id()->set_id(curr_op->id());
                }
            }

            if (std::count(printedNodes.begin(), printedNodes.end(), op->nodeID) == 0)
            {
                if (op->out_outer_crd.defined())
                {
                    edgeType = "crd";
                    printComment = true;
                    comment = "outer-" + op->outer.getName();
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    curr_op->mutable_coord_hold()->mutable_output_outer_crd()->set_name(comment);
                    out_stream.cstream = curr_op->mutable_coord_hold()->mutable_output_outer_crd();
                    out_stream.type = "crd";
                    op->out_outer_crd.accept(this);
                }

                if (op->out_inner_crd.defined())
                {
                    edgeType = "crd";
                    printComment = true;
                    comment = "inner-" + op->inner.getName();
                    // os << tab << op->nodeID << " -> ";
                    curr_op = id_to_op[op->nodeID + 1];
                    curr_op->mutable_coord_hold()->mutable_output_inner_crd()->set_name(comment);
                    out_stream.cstream = curr_op->mutable_coord_hold()->mutable_output_inner_crd();
                    out_stream.type = "crd";
                    op->out_inner_crd.accept(this);
                }

                edgeType = "";
            }
            printedNodes.push_back(op->nodeID);
        }

        string SAMDotEdgePrinter::printerHelper()
        {
            stringstream ss;
            // ss << " [";
            string labelExt = printComment && !comment.empty() ? "_" + comment : "";

            // ss << "label=\"" << (edgeType.empty() ? "val" : edgeType) << labelExt << "\"";
            label = (edgeType.empty() ? "val" : edgeType) + labelExt;
            if (prettyPrint)
            {
                // ss << edgeStyle[edgeType];
            }
            if (printAttributes)
            {
                // ss << " type=\"" << (edgeType.empty() ? "val" : edgeType) << "\"";
            }
            if (printComment)
            {
                // ss << " comment=\"" << comment << "\"";
            }
            // ss << "]";

            printComment = false;
            comment = "";
            return ss.str();
        }

        void SAMDotEdgePrinter::setPrintAttributes(bool printAttributes)
        {
            this->printAttributes = printAttributes;
        }
    }
}
