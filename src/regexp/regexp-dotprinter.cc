// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-dotprinter.h"

#include "src/regexp/regexp-compiler.h"
#include "src/utils/ostreams.h"
#include "src/utils/splay-tree-inl.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------
// Dot/dotty output

#ifdef DEBUG

class DotPrinterImpl : public NodeVisitor {
 public:
  DotPrinterImpl(std::ostream& os, bool ignore_case)  // NOLINT
      : os_(os), ignore_case_(ignore_case) {}
  void PrintNode(const char* label, RegExpNode* node);
  void Visit(RegExpNode* node);
  void PrintAttributes(RegExpNode* from);
  void PrintOnFailure(RegExpNode* from, RegExpNode* to);
#define DECLARE_VISIT(Type) virtual void Visit##Type(Type##Node* that);
  FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
 private:
  std::ostream& os_;
  bool ignore_case_;
};

void DotPrinterImpl::PrintNode(const char* label, RegExpNode* node) {
  os_ << "digraph G {\n  graph [label=\"";
  for (int i = 0; label[i]; i++) {
    switch (label[i]) {
      case '\\':
        os_ << "\\\\";
        break;
      case '"':
        os_ << "\"";
        break;
      default:
        os_ << label[i];
        break;
    }
  }
  os_ << "\"];\n";
  Visit(node);
  os_ << "}" << std::endl;
}

void DotPrinterImpl::Visit(RegExpNode* node) {
  if (node->info()->visited) return;
  node->info()->visited = true;
  node->Accept(this);
}

void DotPrinterImpl::PrintOnFailure(RegExpNode* from, RegExpNode* on_failure) {
  os_ << "  n" << from << " -> n" << on_failure << " [style=dotted];\n";
  Visit(on_failure);
}

class TableEntryBodyPrinter {
 public:
  TableEntryBodyPrinter(std::ostream& os, ChoiceNode* choice)  // NOLINT
      : os_(os), choice_(choice) {}
  void Call(uc16 from, DispatchTable::Entry entry) {
    OutSet* out_set = entry.out_set();
    for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
      if (out_set->Get(i)) {
        os_ << "    n" << choice() << ":s" << from << "o" << i << " -> n"
            << choice()->alternatives()->at(i).node() << ";\n";
      }
    }
  }

 private:
  ChoiceNode* choice() { return choice_; }
  std::ostream& os_;
  ChoiceNode* choice_;
};

class TableEntryHeaderPrinter {
 public:
  explicit TableEntryHeaderPrinter(std::ostream& os)  // NOLINT
      : first_(true), os_(os) {}
  void Call(uc16 from, DispatchTable::Entry entry) {
    if (first_) {
      first_ = false;
    } else {
      os_ << "|";
    }
    os_ << "{\\" << AsUC16(from) << "-\\" << AsUC16(entry.to()) << "|{";
    OutSet* out_set = entry.out_set();
    int priority = 0;
    for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
      if (out_set->Get(i)) {
        if (priority > 0) os_ << "|";
        os_ << "<s" << from << "o" << i << "> " << priority;
        priority++;
      }
    }
    os_ << "}}";
  }

 private:
  bool first_;
  std::ostream& os_;
};

class AttributePrinter {
 public:
  explicit AttributePrinter(std::ostream& os)  // NOLINT
      : os_(os), first_(true) {}
  void PrintSeparator() {
    if (first_) {
      first_ = false;
    } else {
      os_ << "|";
    }
  }
  void PrintBit(const char* name, bool value) {
    if (!value) return;
    PrintSeparator();
    os_ << "{" << name << "}";
  }
  void PrintPositive(const char* name, int value) {
    if (value < 0) return;
    PrintSeparator();
    os_ << "{" << name << "|" << value << "}";
  }

 private:
  std::ostream& os_;
  bool first_;
};

void DotPrinterImpl::PrintAttributes(RegExpNode* that) {
  os_ << "  a" << that << " [shape=Mrecord, color=grey, fontcolor=grey, "
      << "margin=0.1, fontsize=10, label=\"{";
  AttributePrinter printer(os_);
  NodeInfo* info = that->info();
  printer.PrintBit("NI", info->follows_newline_interest);
  printer.PrintBit("WI", info->follows_word_interest);
  printer.PrintBit("SI", info->follows_start_interest);
  Label* label = that->label();
  if (label->is_bound()) printer.PrintPositive("@", label->pos());
  os_ << "}\"];\n"
      << "  a" << that << " -> n" << that
      << " [style=dashed, color=grey, arrowhead=none];\n";
}

static const bool kPrintDispatchTable = false;
void DotPrinterImpl::VisitChoice(ChoiceNode* that) {
  if (kPrintDispatchTable) {
    os_ << "  n" << that << " [shape=Mrecord, label=\"";
    TableEntryHeaderPrinter header_printer(os_);
    that->GetTable(ignore_case_)->ForEach(&header_printer);
    os_ << "\"]\n";
    PrintAttributes(that);
    TableEntryBodyPrinter body_printer(os_, that);
    that->GetTable(ignore_case_)->ForEach(&body_printer);
  } else {
    os_ << "  n" << that << " [shape=Mrecord, label=\"?\"];\n";
    for (int i = 0; i < that->alternatives()->length(); i++) {
      GuardedAlternative alt = that->alternatives()->at(i);
      os_ << "  n" << that << " -> n" << alt.node();
    }
  }
  for (int i = 0; i < that->alternatives()->length(); i++) {
    GuardedAlternative alt = that->alternatives()->at(i);
    alt.node()->Accept(this);
  }
}

void DotPrinterImpl::VisitText(TextNode* that) {
  Zone* zone = that->zone();
  os_ << "  n" << that << " [label=\"";
  for (int i = 0; i < that->elements()->length(); i++) {
    if (i > 0) os_ << " ";
    TextElement elm = that->elements()->at(i);
    switch (elm.text_type()) {
      case TextElement::ATOM: {
        Vector<const uc16> data = elm.atom()->data();
        for (int i = 0; i < data.length(); i++) {
          os_ << static_cast<char>(data[i]);
        }
        break;
      }
      case TextElement::CHAR_CLASS: {
        RegExpCharacterClass* node = elm.char_class();
        os_ << "[";
        if (node->is_negated()) os_ << "^";
        for (int j = 0; j < node->ranges(zone)->length(); j++) {
          CharacterRange range = node->ranges(zone)->at(j);
          os_ << AsUC16(range.from()) << "-" << AsUC16(range.to());
        }
        os_ << "]";
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  os_ << "\", shape=box, peripheries=2];\n";
  PrintAttributes(that);
  os_ << "  n" << that << " -> n" << that->on_success() << ";\n";
  Visit(that->on_success());
}

void DotPrinterImpl::VisitBackReference(BackReferenceNode* that) {
  os_ << "  n" << that << " [label=\"$" << that->start_register() << "..$"
      << that->end_register() << "\", shape=doubleoctagon];\n";
  PrintAttributes(that);
  os_ << "  n" << that << " -> n" << that->on_success() << ";\n";
  Visit(that->on_success());
}

void DotPrinterImpl::VisitEnd(EndNode* that) {
  os_ << "  n" << that << " [style=bold, shape=point];\n";
  PrintAttributes(that);
}

void DotPrinterImpl::VisitAssertion(AssertionNode* that) {
  os_ << "  n" << that << " [";
  switch (that->assertion_type()) {
    case AssertionNode::AT_END:
      os_ << "label=\"$\", shape=septagon";
      break;
    case AssertionNode::AT_START:
      os_ << "label=\"^\", shape=septagon";
      break;
    case AssertionNode::AT_BOUNDARY:
      os_ << "label=\"\\b\", shape=septagon";
      break;
    case AssertionNode::AT_NON_BOUNDARY:
      os_ << "label=\"\\B\", shape=septagon";
      break;
    case AssertionNode::AFTER_NEWLINE:
      os_ << "label=\"(?<=\\n)\", shape=septagon";
      break;
  }
  os_ << "];\n";
  PrintAttributes(that);
  RegExpNode* successor = that->on_success();
  os_ << "  n" << that << " -> n" << successor << ";\n";
  Visit(successor);
}

void DotPrinterImpl::VisitAction(ActionNode* that) {
  os_ << "  n" << that << " [";
  switch (that->action_type_) {
    case ActionNode::SET_REGISTER:
      os_ << "label=\"$" << that->data_.u_store_register.reg
          << ":=" << that->data_.u_store_register.value << "\", shape=octagon";
      break;
    case ActionNode::INCREMENT_REGISTER:
      os_ << "label=\"$" << that->data_.u_increment_register.reg
          << "++\", shape=octagon";
      break;
    case ActionNode::STORE_POSITION:
      os_ << "label=\"$" << that->data_.u_position_register.reg
          << ":=$pos\", shape=octagon";
      break;
    case ActionNode::BEGIN_SUBMATCH:
      os_ << "label=\"$" << that->data_.u_submatch.current_position_register
          << ":=$pos,begin\", shape=septagon";
      break;
    case ActionNode::POSITIVE_SUBMATCH_SUCCESS:
      os_ << "label=\"escape\", shape=septagon";
      break;
    case ActionNode::EMPTY_MATCH_CHECK:
      os_ << "label=\"$" << that->data_.u_empty_match_check.start_register
          << "=$pos?,$" << that->data_.u_empty_match_check.repetition_register
          << "<" << that->data_.u_empty_match_check.repetition_limit
          << "?\", shape=septagon";
      break;
    case ActionNode::CLEAR_CAPTURES: {
      os_ << "label=\"clear $" << that->data_.u_clear_captures.range_from
          << " to $" << that->data_.u_clear_captures.range_to
          << "\", shape=septagon";
      break;
    }
  }
  os_ << "];\n";
  PrintAttributes(that);
  RegExpNode* successor = that->on_success();
  os_ << "  n" << that << " -> n" << successor << ";\n";
  Visit(successor);
}

class DispatchTableDumper {
 public:
  explicit DispatchTableDumper(std::ostream& os) : os_(os) {}
  void Call(uc16 key, DispatchTable::Entry entry);

 private:
  std::ostream& os_;
};

void DispatchTableDumper::Call(uc16 key, DispatchTable::Entry entry) {
  os_ << "[" << AsUC16(key) << "-" << AsUC16(entry.to()) << "]: {";
  OutSet* set = entry.out_set();
  bool first = true;
  for (unsigned i = 0; i < OutSet::kFirstLimit; i++) {
    if (set->Get(i)) {
      if (first) {
        first = false;
      } else {
        os_ << ", ";
      }
      os_ << i;
    }
  }
  os_ << "}\n";
}

void DispatchTable::Dump() {
  OFStream os(stderr);
  DispatchTableDumper dumper(os);
  tree()->ForEach(&dumper);
}

#endif  // DEBUG

void DotPrinter::DotPrint(const char* label, RegExpNode* node,
                          bool ignore_case) {
#ifdef DEBUG
  StdoutStream os;
  DotPrinterImpl printer(os, ignore_case);
  printer.PrintNode(label, node);
#endif  // DEBUG
}

}  // namespace internal
}  // namespace v8
