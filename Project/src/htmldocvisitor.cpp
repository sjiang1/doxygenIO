/******************************************************************************
 *
 * 
 *
 *
 * Copyright (C) 1997-2015 by Dimitri van Heesch.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation under the terms of the GNU General Public License is hereby 
 * granted. No representations are made about the suitability of this software 
 * for any purpose. It is provided "as is" without express or implied warranty.
 * See the GNU General Public License for more details.
 *
 * Documents produced by Doxygen are derivative works derived from the
 * input used in their production; they are not affected by this license.
 *
 */

#include <sstream> 
#include <qdir.h>
#include <fstream>
#include <algorithm>
#include "htmldocvisitor.h"
#include "docparser.h"
#include "language.h"
#include "doxygen.h"
#include "outputgen.h"
#include "dot.h"
#include "message.h"
#include "config.h"
#include "htmlgen.h"
#include "parserintf.h"
#include "msc.h"
#include "dia.h"
#include "util.h"
#include "vhdldocgen.h"
#include "filedef.h"
#include "memberdef.h"
#include "htmlentity.h"
#include "plantuml.h"
#include "ftvhelp.h"

static const int NUM_HTML_LIST_TYPES = 4;
static const char types[][NUM_HTML_LIST_TYPES] = {"1", "a", "i", "A"};

static QCString convertIndexWordToAnchor(const QString &word)
{
  static char hex[] = "0123456789abcdef";
  QCString result="a";
  const char *str = word.data();
  unsigned char c;
  if (str)
  {
    while ((c = *str++))
    {
      if ((c >= 'a' && c <= 'z') || // ALPHA
          (c >= 'A' && c <= 'A') || // ALPHA
          (c >= '0' && c <= '9') || // DIGIT
          c == '-' ||
          c == '.' ||
          c == '_'
         )
      {
        result += c;
      }
      else
      {
        char enc[4];
        enc[0] = ':';
        enc[1] = hex[(c & 0xf0) >> 4];
        enc[2] = hex[c & 0xf];
        enc[3] = 0;
        result += enc;
      }
    }
  }
  return result;
}

static bool mustBeOutsideParagraph(DocNode *n)
{
  switch (n->kind())
  {
          /* <ul> */
        case DocNode::Kind_HtmlList:
        case DocNode::Kind_SimpleList:
        case DocNode::Kind_AutoList:
          /* <dl> */
        case DocNode::Kind_SimpleSect:
        case DocNode::Kind_ParamSect:
        case DocNode::Kind_HtmlDescList:
        case DocNode::Kind_XRefItem:
          /* <table> */
        case DocNode::Kind_HtmlTable:
          /* <h?> */
        case DocNode::Kind_Section:
        case DocNode::Kind_HtmlHeader:
          /* \internal */
        case DocNode::Kind_Internal:
          /* <div> */
        case DocNode::Kind_Include:
        case DocNode::Kind_Image:
        case DocNode::Kind_SecRefList:
          /* <hr> */
        case DocNode::Kind_HorRuler:
          /* CopyDoc gets paragraph markers from the wrapping DocPara node,
           * but needs to insert them for all documentation being copied to
           * preserve formatting.
           */
        case DocNode::Kind_Copy:
          /* <blockquote> */
        case DocNode::Kind_HtmlBlockQuote:
          /* \parblock */
        case DocNode::Kind_ParBlock:
          return TRUE;
        case DocNode::Kind_Verbatim:
          {
            DocVerbatim *dv = (DocVerbatim*)n;
            return dv->type()!=DocVerbatim::HtmlOnly || dv->isBlock();
          }
        case DocNode::Kind_StyleChange:
          return ((DocStyleChange*)n)->style()==DocStyleChange::Preformatted ||
                 ((DocStyleChange*)n)->style()==DocStyleChange::Div ||
                 ((DocStyleChange*)n)->style()==DocStyleChange::Center;
        case DocNode::Kind_Formula:
          return !((DocFormula*)n)->isInline();
        default:
          break;
  }
  return FALSE;
}

static QString htmlAttribsToString(const HtmlAttribList &attribs)
{
  QString result;
  HtmlAttribListIterator li(attribs);
  HtmlAttrib *att;
  for (li.toFirst();(att=li.current());++li)
  {
    if (!att->value.isEmpty())  // ignore attribute without values as they
                                // are not XHTML compliant
    {
      result+=" ";
      result+=att->name;
      result+="=\""+convertToXML(att->value)+"\"";
    }
  }
  return result;
}

//-------------------------------------------------------------------------

HtmlDocVisitor::HtmlDocVisitor(FTextStream &t,CodeOutputInterface &ci,
                               Definition *ctx) 
  : DocVisitor(DocVisitor_Html), m_t(t), m_ci(ci), m_insidePre(FALSE), 
                                 m_hide(FALSE), m_ctx(ctx)
{
  if (ctx) m_langExt=ctx->getDefFileExtension();
}

  //--------------------------------------
  // visitor functions for leaf nodes
  //--------------------------------------

void HtmlDocVisitor::visit(DocWord *w)
{
  //printf("word: %s\n",w->word().data());
  if (m_hide) return;
  filter(w->word());
}

// Siyuan, added a function for visiting svg docnode
void HtmlDocVisitor::visit(DocSvg *s)
{
  if (m_hide) return;
  float width = s->rectWidth() * 0.6;
  int intWidth = int(width + 0.5);
  std::ostringstream stm;
  stm << intWidth;

  m_t << "&nbsp;<svg width=\"60\" height=\"13\"><rect x=\"0\" y=\"0\" width=\"";
  m_t << stm.str().c_str();
  m_t << "\" height=\"13\" style=\"fill:rgb(0,0,255);stroke-width:1;stroke:rgb(0,0,0)\"></rect>";

  int leftIntWidth=60 - intWidth;
  stm.str("");
  stm.clear();
  stm << leftIntWidth;
  m_t << "<rect x=\"";
  m_t << intWidth;
  m_t << "\" y=\"0\" width=\"";
  m_t << stm.str().c_str();
  m_t << "\" height=\"13\" style=\"fill:rgb(255,255,255);stroke-width:1;stroke:rgb(0,0,0)\"></rect>";
  m_t << "Sorry, your browser does not support inline SVG.</svg>";
}

std::string my_to_string(int val){
  std::ostringstream stm ;
  stm << val ;
  return stm.str() ;
}

// Siyusn: private function for generate an indented row for visit DocVariableValue
// This function directly push strings to m_t
std::string HtmlDocVisitor::generateIndentTableRow(const char * first_column_txt, const char * second_column_txt, const char * third_column_txt, std::string id_prefix, int indent, int index, int display, int collapsible){
  
  std::string indent_str = my_to_string(indent);
  char const *indent_pchar = indent_str.c_str();
  std::string index_str = my_to_string(index);

  std::string id_prefix_string (id_prefix);
  std::string underscore_string ("_");
  // printf("id_prefix_string: %s\n", id_prefix_string.c_str());
  // printf("index_str: %s\n", index_str.c_str());
  std::string current_id_string = id_prefix_string + index_str + underscore_string;

  // start row
  if(display == 1)
    m_t << "<tr id=\"row_" << current_id_string.c_str()<< "\" class=\"even\">";
  else if(display == 0)
    m_t << "<tr id=\"row_" << current_id_string.c_str()<< "\" class=\"\" style=\"display: none;\">";

  // first column 
  m_t << "<td class=\"entry\">";
  // first column -- indention
  m_t << "<span style=\"width:" << indent_pchar << "px;display:inline-block;\">&nbsp;</span>";
  if(collapsible){
    // printf("current_id_string: %s\n", current_id_string.c_str());
    // first column -- arrow
    m_t << "<span id=\"arr_" << current_id_string.c_str() <<  "\" class=\"arrow\" onclick=\"toggleFolder('"
	<< current_id_string.c_str() << "')\">&#9658;</span>";
  }
  // first column -- txt
  if(strcmp(first_column_txt, "$return_value") == 0){
    m_t << "return";
  }
  else{
    m_t << first_column_txt;
  }
  m_t << "</td>";

  // second column
  m_t <<"<td class=\"desc\">";
  m_t << second_column_txt;
  m_t <<"</td>";

  //third column
  m_t <<"<td class=\"desc\">";
  m_t << third_column_txt;
  m_t <<"</td>";

  //end row
  m_t << "</tr>";

  return current_id_string;
}

// trim from start (in place)
static inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

std::string to_string( const int& n )
{
  std::ostringstream stm ;
  stm << n ;
  return stm.str() ;
}

// Docio: private function for generating a row for a value based on previous row's name
void HtmlDocVisitor::generateRow(std::vector<std::string> &previous_parameter_name_list,
				 std::stack<std::string> &previous_id_stack,
				 std::stack<int> &previous_index_stack,
				 std::vector<std::string> &parameter_name_list,
				 int& indent, int &index, std::string &previous_id,
				 const char*parameter_name_cstr, const char*parameter_value_cstr,
				 const char*ret_parameter_value_cstr, int collapsible){
  /* printf("previous_parameter_list size: %d, current parameter name list size: %d\n",
     previous_parameter_name_list.size(), parameter_name_list.size()); */
  
  int is_shown = 0;
  if(previous_id_stack.size() == 1)
    is_shown = 1;

  int indent_unit = 16;
  
  if(previous_parameter_name_list.size() == 0){
    // printf("branch 1\n");
    indent = 0;
    index =previous_index_stack.top();
    previous_index_stack.pop();
    index = index + 1;
    previous_id = generateIndentTableRow(parameter_name_cstr, parameter_value_cstr, ret_parameter_value_cstr,
					 previous_id_stack.top(), indent, index, is_shown, collapsible);
    previous_index_stack.push(index);
  }
  else if(previous_parameter_name_list.size() == parameter_name_list.size()){
    // printf("branch 2\n");
    index = previous_index_stack.top();
    previous_index_stack.pop();
    index = index + 1;
    previous_id = generateIndentTableRow(parameter_name_cstr, parameter_value_cstr, ret_parameter_value_cstr,
					 previous_id_stack.top(), indent, index, is_shown, collapsible);
    // printf("pushed index: %d\n", index);
    previous_index_stack.push(index);
  }
  else if(previous_parameter_name_list.size() >  parameter_name_list.size()){
    // printf("branch 3\n");
    int level_gap = previous_parameter_name_list.size() - parameter_name_list.size();
    // printf("level_gap: %d\n", level_gap);
    indent = indent - indent_unit*level_gap;
    // printf("new indent: %d\n", indent);
    index = previous_index_stack.top();
    // printf("current index: %d\n", index);
    previous_index_stack.pop();
    for(int i = 0; i < level_gap; i++){
      // printf("loop i=%d\n", i);
      index = previous_index_stack.top();
      previous_index_stack.pop();
      previous_id_stack.pop();
    }
    index = index + 1;
    if(previous_id_stack.size() == 1)
      is_shown = 1;
    previous_id = generateIndentTableRow(parameter_name_cstr, parameter_value_cstr, ret_parameter_value_cstr,
					 previous_id_stack.top(), indent, index, is_shown, collapsible);
    previous_index_stack.push(index);
  }
  else if(previous_parameter_name_list.size() <  parameter_name_list.size()){
    // printf("branch 4\n");
    indent += indent_unit;
    index = 0;
    previous_id_stack.push(previous_id);
    previous_id = generateIndentTableRow(parameter_name_cstr, parameter_value_cstr, ret_parameter_value_cstr,
					 previous_id_stack.top(), indent, index, 0, collapsible);
    previous_index_stack.push(index);
  }
  return;
}

// Docio: private function for process one line
void HtmlDocVisitor::processOneline(std::string next_line, std::string line,
				    std::string ret_line, const char* parameter_name_to_process,
				    int &indent, int &index,
				    std::string &previous_id,
				    std::stack<std::string> &previous_id_stack,
				    std::stack<int> &previous_index_stack,
				    std::vector<std::string> &previous_parameter_name_list){
  std::string parameter_full_name, parameter_name, parameter_value, ret_parameter_name, ret_parameter_value;
  
  std::istringstream ss(line);
  std::istringstream retss(ret_line);

  // get parameter name from the line
  std::getline(ss, parameter_full_name, '\t');
  std::getline(retss, ret_parameter_name, '\t');
  /* printf("Full name: %s\t", parameter_full_name.c_str());
     printf("line: %s\n", line.c_str());
     printf("ret_line: %s\n", ret_line.c_str()); */
  
  // get parameter value from the line
  std::getline(ss, parameter_value, '\t');
  std::getline(retss, ret_parameter_value, '\t');

  std::istringstream nss(parameter_full_name);
  // printf("Parameter full name: %s\n", parameter_full_name.c_str());
  std::vector<std::string> parameter_name_list;
  
  int isFirstSubname = 1;
  // split full name into a list names by '.', '*', '->'
  std::istringstream parameter_name_ss, subname_ss;
  std::string subname, subname2;
  while(std::getline(nss, parameter_name, '*')){
    parameter_name_ss.str(parameter_name);
    while(std::getline(parameter_name_ss, subname, '.')){
      
      size_t pos = 0;
      while ((pos = subname.find("->")) != std::string::npos) {
	subname2 = subname.substr(0, pos);
	subname.erase(0, pos + 2);
	trim(subname2);

	if(isFirstSubname && parameter_name_to_process){
	  isFirstSubname = 0;
	  printf("first subname: %s\t", subname2.c_str());
	  printf("to match: %s\n", parameter_name_to_process);
	  if(strcmp(subname2.c_str(), parameter_name_to_process) != 0)
	    return;
	}
	
	parameter_name_list.push_back(subname2);
      }

      if(isFirstSubname && parameter_name_to_process){
	trim(subname);
	isFirstSubname = 0;
	printf("first subname: %s\t", subname.c_str());
	printf("to match: %s\n", parameter_name_to_process);
	if(strcmp(subname.c_str(), parameter_name_to_process) != 0)
	  return;

	parameter_name_list.push_back(subname);
      }
      
      
    }
    parameter_name_ss.clear();
  }

  int collapsible = 0;
  if(!next_line.empty()){
    // printf("next line is not empty!\n");
    std::istringstream next_ss(next_line);
    std::string next_parameter_full_name;
    std::vector<std::string> next_parameter_name_list;
    std::getline(next_ss, next_parameter_full_name, ',');
    std::istringstream next_nss(next_parameter_full_name);
    std::string tmpstring, tmpstring2, tmpstring3;
    while(std::getline(next_nss, tmpstring, '*')){
      std::istringstream next_subname_ss(tmpstring);

      while(std::getline(next_subname_ss, tmpstring2, '.')){

	size_t pos = 0;
	while ((pos = tmpstring2.find("->")) != std::string::npos) {
	  tmpstring3 = tmpstring2.substr(0, pos);
	  tmpstring2.erase(0, pos + 2);
	  trim(tmpstring3);
	  next_parameter_name_list.push_back(tmpstring3);
	}
	
      }

      next_subname_ss.clear();
    }
    if(next_parameter_name_list.size() > parameter_name_list.size())
      collapsible = 1;
  }

  // printf("Collapsible: %d\n", collapsible);
  generateRow(previous_parameter_name_list, previous_id_stack, previous_index_stack,
	      parameter_name_list, indent, index, previous_id,
	      parameter_name.c_str(), parameter_value.c_str(),
	      ret_parameter_value.c_str(), collapsible);
  
  previous_parameter_name_list = parameter_name_list;
}

// Docio: added a function for visiting value node
void HtmlDocVisitor::visit(DocVariableValue *v){
  
  if (m_hide) return;

  QCString funcname = v->funcname();
  std::ifstream infile("ioexamples/" + funcname + ".parameter.example.i");
  std::ifstream outfile("ioexamples/" + funcname + ".parameter.example.o");
  std::ifstream parameterIdsFile("parameterids.txt");

  if ( infile.peek() == std::ifstream::traits_type::eof()
       && outfile.peek() == std::ifstream::traits_type::eof()) {
    return;
  }

  if ( parameterIdsFile.peek() == std::ifstream::traits_type::eof() ){
    return;
  }
  
  int indent = 0;
  int index = 0;
  
  // get the parameter's id
  std::string previous_id = to_string(1) + "_";
  HtmlDocVisitor::getParameterId(parameterIdsFile, previous_id, v->paramname(), funcname);

  std::stack<std::string> previous_id_stack;
  std::stack<int> previous_index_stack;
  m_t << "<table class=\"fieldtable\"><tbody>";
  m_t << "<tr><th>parameter name</th><th>value when function called</th><th>value when function returns</th></tr>";
  std::string line, next_line, ret_line;
  std::vector<std::string> previous_parameter_name_list;

  int passed_return = 0;

  // set previous id
  // previous_id_stack.push(to_string(1)+"_");
  previous_id_stack.push(previous_id);
  // previous_id_stack.push(to_string(function_id)+"_"+to_string(parameter_id)+"_");
  previous_index_stack.push(-1);
  
  // read the first line
  std::getline(infile, line);
    
  // read file line by line
  while (std::getline(infile, next_line)){
    std::getline(outfile, ret_line);
    processOneline(next_line, line, ret_line, v->paramname(),
		   indent, index,
		   previous_id,
		   previous_id_stack,
		   previous_index_stack,
		   previous_parameter_name_list);
    
    line = next_line; // update the current line
  }//finish read file line by line

  // process the last line
  std::getline(outfile, ret_line);
  next_line.clear();
  processOneline(next_line, line, ret_line, v->paramname(),
		 indent, index,
		 previous_id,
		 previous_id_stack,
		 previous_index_stack,
		 previous_parameter_name_list);
  
  m_t << "</tbody></table>";
}

void HtmlDocVisitor::getParameterId(std::ifstream &parameterIdsFile, std::string &previous_id,
				    QCString paramName, QCString funcname){
  std::string id_line;
  while(std::getline(parameterIdsFile, id_line)){
    std::string functionid, parameterid, functionname, parametertype, parameter_name_inidfile;
    std::istringstream idline_ss(id_line);
    std::getline(idline_ss, functionid, '\t');
    std::getline(idline_ss, parameterid, '\t');
    std::getline(idline_ss, functionname, '\t');
    std::getline(idline_ss, parametertype, '\t');
    std::getline(idline_ss, parameter_name_inidfile, '\t');
    if(strcmp(parameter_name_inidfile.c_str(),  paramName) == 0
       && strcmp(functionname.c_str(), funcname) == 0){
      previous_id = functionid + "_" + parameterid + "_";
      break;
    }
  }
  return;
}

// Docio: private function to get parameter subnames
std::vector<std::string> HtmlDocVisitor::getSubNames(std::string parameter_full_name){
  // split full name into a list names by '.', '*', '->'

  std::vector<std::string> parameter_name_list;

  std::istringstream nss(parameter_full_name);
  std::istringstream parameter_name_ss;
  std::string parameter_name;
  while(std::getline(nss, parameter_name, '*')){
    
    parameter_name_ss.str(parameter_name);
    std::string subname;
    while(std::getline(parameter_name_ss, subname, '.')){

      size_t pos = 0;
      while ((pos = subname.find("->")) != std::string::npos) {
	
	std::string subname2 = subname.substr(0, pos);
	trim(subname2);
	if(subname2.compare("") != 0){

	  parameter_name_list.push_back(subname2);
	}
	
	subname.erase(0, pos + 2);
      }

      trim(subname);
      if(subname.compare("") != 0 ){
	parameter_name_list.push_back(subname);
      }
      
    }
    parameter_name_ss.clear();
  }

  return parameter_name_list;
}

void HtmlDocVisitor::getFunctionId(std::ifstream &parameterIdsFile, std::string &previous_id,
				   QCString funcname){
  std::string id_line;
  while(std::getline(parameterIdsFile, id_line)){
    std::string functionid, parameterid, functionname, parametertype, parameter_name_inidfile;
    std::istringstream idline_ss(id_line);
    std::getline(idline_ss, functionid, '\t');
    std::getline(idline_ss, parameterid, '\t');
    std::getline(idline_ss, functionname, '\t');
    std::getline(idline_ss, parametertype, '\t');
    std::getline(idline_ss, parameter_name_inidfile, '\t');
    if(strcmp(functionname.c_str(), funcname) == 0){
      previous_id = functionid + "_";
      return;
    }
  }
  previous_id = "";
}

std::string HtmlDocVisitor::getParameterName(std::string line){
  if(line.compare("") == 0) return "";

  std::size_t found = line.find_first_of('\t');
  std::string tmp = line.substr(0, found);
  trim(tmp);
  return tmp;
}

std::string HtmlDocVisitor::getParameterValue(std::string line){
  if(line.compare("") == 0) return "";
  
  std::size_t found = line.find_first_of('\t');
  std::string tmp = line.substr(found+1);
  trim(tmp);
  return tmp;
}


int HtmlDocVisitor::is_collapsible(std::vector<std::string> current_list, std::vector<std::string> next_list,
				   std::string current_name, std::string next_name){

  return next_list.size() > current_list.size();
}

int HtmlDocVisitor::is_dereference(std::vector<std::string> current_list, std::vector<std::string> next_list,
				 std::string current_name, std::string next_name){
  if(next_list.size() != current_list.size())
    return 0;
  
  if(next_name.at(0) != '*')
    return 0;
  
  std::string tmp = next_name.substr(1);
  return tmp.compare(current_name) == 0 ;
}

int HtmlDocVisitor::is_skippable(std::vector<std::string> current_list, std::vector<std::string> next_list,
				 std::string current_name, std::string next_name){
  // example, current parameter: p, next parameter *p, in this case,
  // we show only *p
  if(SHOW_DEREFD_POINTER)
    return 0;
  
  return is_dereference(current_list, next_list, current_name, next_name);
}

/*!
 * visualize io values from files
 */
void HtmlDocVisitor::visualizeIovalues(std::ifstream &infile, std::ifstream &outfile, std::string function_id){
  std::stack<std::string> 	previous_id_stack;
  std::stack<int> 		previous_index_stack;
  std::vector<std::string> 	previous_parameter_name_list;

  std::string previous_id = function_id;
  previous_id_stack.push(previous_id);
  previous_index_stack.push(-1);
  
  int indent = 0, index = 0, finish_in = 0, finish_out = 0, prev_in_collapsible = 0, prev_out_collapsible = 0;
  std::string line, out_line, next_line, next_out_line;
  
  int read_in_file = 1, read_out_file = 1;
  if(!std::getline(infile, line) || line.compare("") == 0){
    finish_in = 1;
    line = "";
  }
  
  if(!std::getline(outfile, out_line) || out_line.compare("") == 0){
    finish_out = 1;
    out_line = "";
  }
  
  std::string in_name = getParameterName(line);
  std::string out_name = getParameterName(out_line);
  std::string next_in_name = "", next_out_name = "";

  int inline_cnt = 0, outline_cnt = 0;
  while (!finish_in || !finish_out){

    int prev_finish_in = finish_in;
    int prev_finish_out = finish_out;
    
    if(!finish_in && read_in_file){
      std::istream &i = std::getline(infile, next_line);
      
      if(!i || next_line.compare("") == 0 ){
	finish_in = 1;
	next_line = "";
      }

      fprintf(stderr, "i:%d ", inline_cnt);
      inline_cnt ++;
      next_in_name = getParameterName(next_line);
    }
    
    if(!finish_out && read_out_file){
      std::istream &o = std::getline(outfile, next_out_line);
      
      if(!o || next_out_line.compare("") == 0){
	finish_out = 1;
	next_out_line = "";
      }

      fprintf(stderr, "o:%d ", outline_cnt);
      outline_cnt ++;
      next_out_name = getParameterName(next_out_line);
    }
    
    std::string parameter_name, in_value, out_value;
    // 0: uninitialized,
    // 1: process both lines from in&out files,
    // 2: process in file, 3: process out file
    int process_type = 0; 

    if(prev_finish_in){
      process_type = 3;
      read_in_file = 0;
      read_out_file = 1;

      parameter_name = out_name;
      out_value = getParameterValue(out_line);
    }
    else if(prev_finish_out){
      process_type = 2;
      
      read_in_file = 1;
      read_out_file = 0;

      parameter_name = in_name;
      in_value = getParameterValue(line);
    }
    else if(in_name.compare(out_name) == 0){
      process_type = 1;
      read_in_file = read_out_file = 1;
      
      // if the two lines have the same parameter name
      parameter_name = in_name;
      in_value = getParameterValue(line);
      out_value = getParameterValue(out_line);
    }
    else if(prev_in_collapsible){
      process_type = 2;
      
      read_in_file = 1;
      read_out_file = 0;

      parameter_name = in_name;
      in_value = getParameterValue(line);
    }
    else if(prev_out_collapsible){
      process_type = 3;
      
      read_in_file = 0;
      read_out_file = 1;

      parameter_name = out_name;
      out_value = getParameterValue(out_line);
    }

    std::vector<std::string> parameter_name_list = getSubNames(parameter_name);
    std::vector<std::string> next_in_name_list, next_out_name_list;
    
    int skip = 0;
    // check the current row should be skipped or not
    int tmp1 = 1, tmp2 = 1;
    if(process_type == 1 || process_type == 2){
      next_in_name_list = getSubNames(next_in_name);
      if(finish_in){
	tmp1 = 0;
      }
      else{
	tmp1 = is_skippable(parameter_name_list, next_in_name_list, parameter_name, next_in_name);
      }
    }
    
    if(process_type == 1 || process_type == 3){
      next_out_name_list = getSubNames(next_out_name);
      if(finish_out){
	tmp2 = 0;
      }
      else{
	tmp2 = is_skippable(parameter_name_list, next_out_name_list, parameter_name, next_out_name);
      }
    }
    skip = tmp1 && tmp2;

    // check if the current row should be collapsed (table row)
    int in_collapsible = 0, out_collapsible = 0;
    int collapsible = 0;
    if(!skip){
      
      if(process_type == 1 || process_type == 2){
	if(finish_in){
	  in_collapsible = 0;
	}
	else{
	  in_collapsible = is_collapsible(parameter_name_list, next_in_name_list, parameter_name, next_in_name);
	}
      }
      if(process_type == 1 || process_type == 3){
	if(finish_out){
	  out_collapsible = 0;
	}
	else{
	  out_collapsible = is_collapsible(parameter_name_list, next_out_name_list, parameter_name, next_out_name);
	}
      }

      if(process_type == 1){
	collapsible = in_collapsible || out_collapsible;
      }
      else if(process_type == 2){
	collapsible = in_collapsible;
      }
      else if(process_type == 3){
	collapsible = out_collapsible;
      }
      
    }
    

    if (!skip && process_type == 1){
      generateRow(previous_parameter_name_list, previous_id_stack, previous_index_stack,
		  parameter_name_list, indent, index, previous_id,
		  parameter_name.c_str(), in_value.c_str(), out_value.c_str(), collapsible);
    }
    else if(!skip && process_type == 2){
      generateRow(previous_parameter_name_list, previous_id_stack, previous_index_stack,
		  parameter_name_list, indent, index, previous_id,
		  parameter_name.c_str(), in_value.c_str(), "", collapsible);
    }
    else if(!skip && process_type == 3){
      generateRow(previous_parameter_name_list, previous_id_stack, previous_index_stack,
		  parameter_name_list, indent, index, previous_id,
		  parameter_name.c_str(), "", out_value.c_str(), collapsible);
    }	


    // update the current line, the names
    if(!finish_in && read_in_file){
      line = next_line;
      in_name = next_in_name;
      prev_in_collapsible = is_dereference(parameter_name_list, next_in_name_list, parameter_name, next_in_name)
	|| in_collapsible;
    }
    
    if(!finish_out && read_out_file){
      out_line = next_out_line;
      out_name = next_out_name;
      prev_out_collapsible = is_dereference(parameter_name_list, next_out_name_list, parameter_name, next_out_name)
	|| out_collapsible;
    }
    
    previous_parameter_name_list = parameter_name_list;
    
  }//finish read file line by line
}

int HtmlDocVisitor::count_lines(const char * file){
  std::ifstream f(file);
  std::string line;
  int i;
  for (i = 0; std::getline(f, line); ++i)
    ;
  f.close();
  
  fprintf(stderr, "%s: %d\n", file, i);
  return i;
}

void HtmlDocVisitor::visit(DocIoexample *io){
  if (m_hide) return;
  QCString funcname = io->funcname();
  std::string funcname_str = qPrint(funcname);
  std::string infile_name = "ioexamples/";
  infile_name.append(funcname_str);
  infile_name.append(".parameter.example.i");
  std::string outfile_name = "ioexamples/";
  outfile_name.append(funcname_str);
  outfile_name.append(".parameter.example.o");
  
  int lineNumber_infile  = count_lines(infile_name.c_str());
  int lineNumber_outfile = count_lines(outfile_name.c_str());

  std::ifstream infile(infile_name.c_str());
  std::ifstream outfile(outfile_name.c_str());
  std::ifstream parameterIdsFile("parameterids.txt");
  if ( infile.peek() == std::ifstream::traits_type::eof()
       || outfile.peek() == std::ifstream::traits_type::eof()
       || parameterIdsFile.peek() == std::ifstream::traits_type::eof()) {
    return;
  }

  if(lineNumber_infile > 200 || lineNumber_outfile > 200 ||
     (lineNumber_infile == 0 && lineNumber_outfile == 0) ){
    std::ofstream outfile;
    outfile.open("functions-over200.txt", std::ios_base::app);
    outfile << qPrint(funcname) << "\n";
    return;
  }
  
  std::ofstream logfile;
  logfile.open("functions-doc.txt", std::ios_base::app);
  logfile << qPrint(funcname) << "\n";
  
  // get the function's id
  std::string function_id = "";
  getFunctionId(parameterIdsFile, function_id, funcname);
  if(function_id.compare("") == 0)
    return;

  fprintf(stderr, "Generating Io example: function %s\n", qPrint(funcname));
  forceEndParagraph(io);
  m_t << "<dl class=\"section ioexample\"><dt>I/O Example</dt><dd>";

  // Table of values of parameters and return
  m_t << "<table class=\"fieldtable\"><tbody>";
  // headline
  m_t << "<tr><th>parameter name</th><th>before function call</th><th>after function call</th></tr>";

  visualizeIovalues(infile, outfile, function_id);
  std::ifstream retfile("ioexamples/" + funcname + ".return.example");
  // at here, we won't read infile any more. because it should be read in the previous line
  visualizeIovalues(infile, retfile, function_id); 
  
  // wrap up the table and the section
  m_t << "</tbody></table></dd></dl>";
  return;
}

void HtmlDocVisitor::visit(DocLinkedWord *w)
{
  if (m_hide) return;
  //printf("linked word: %s\n",w->word().data());
  startLink(w->ref(),w->file(),w->relPath(),w->anchor(),w->tooltip());
  filter(w->word());
  endLink();
}

void HtmlDocVisitor::visit(DocWhiteSpace *w)
{
  if (m_hide) return;
  if (m_insidePre)
  {
    m_t << w->chars();
  }
  else
  {
    m_t << " ";
  }
}

void HtmlDocVisitor::visit(DocSymbol *s)
{
  if (m_hide) return;
  const char *res = HtmlEntityMapper::instance()->html(s->symbol());
  if (res)
  {
    m_t << res;
  }
  else
  {
    err("HTML: non supported HTML-entity found: %s\n",HtmlEntityMapper::instance()->html(s->symbol(),TRUE));
  }
}

void HtmlDocVisitor::writeObfuscatedMailAddress(const QCString &url)
{
  m_t << "<a href=\"#\" onclick=\"location.href='mai'+'lto:'";
  uint i;
  int size=3;
  for (i=0;i<url.length();)
  {
    m_t << "+'" << url.mid(i,size) << "'";
    i+=size;
    if (size==3) size=2; else size=3;
  }
  m_t << "; return false;\">";
}

void HtmlDocVisitor::visit(DocURL *u)
{
  if (m_hide) return;
  if (u->isEmail()) // mail address
  {
    QCString url = u->url();
    writeObfuscatedMailAddress(url);
    uint size=5,i;
    for (i=0;i<url.length();)
    {
      filter(url.mid(i,size));
      if (i<url.length()-size) m_t << "<span style=\"display: none;\">.nosp@m.</span>";
      i+=size;
      if (size==5) size=4; else size=5;
    }
    m_t << "</a>";
  }
  else // web address
  {
    m_t << "<a href=\"";
    m_t << u->url() << "\">";
    filter(u->url());
    m_t << "</a>";
  }
}

void HtmlDocVisitor::visit(DocLineBreak *)
{
  if (m_hide) return;
  m_t << "<br />\n";
}

void HtmlDocVisitor::visit(DocHorRuler *hr)
{
  if (m_hide) return;
  forceEndParagraph(hr);
  m_t << "<hr/>\n";
  forceStartParagraph(hr);
}

void HtmlDocVisitor::visit(DocStyleChange *s)
{
  if (m_hide) return;
  switch (s->style())
  {
    case DocStyleChange::Bold:
      if (s->enable()) m_t << "<b" << htmlAttribsToString(s->attribs()) << ">";      else m_t << "</b>";
      break;
    case DocStyleChange::Italic:
      if (s->enable()) m_t << "<em" << htmlAttribsToString(s->attribs()) << ">";     else m_t << "</em>";
      break;
    case DocStyleChange::Code:
      if (s->enable()) m_t << "<code" << htmlAttribsToString(s->attribs()) << ">";   else m_t << "</code>";
      break;
    case DocStyleChange::Subscript:
      if (s->enable()) m_t << "<sub" << htmlAttribsToString(s->attribs()) << ">";    else m_t << "</sub>";
      break;
    case DocStyleChange::Superscript:
      if (s->enable()) m_t << "<sup" << htmlAttribsToString(s->attribs()) << ">";    else m_t << "</sup>";
      break;
    case DocStyleChange::Center:
      if (s->enable()) 
      {
        forceEndParagraph(s);
        m_t << "<center" << htmlAttribsToString(s->attribs()) << ">"; 
      }
      else 
      {
        m_t << "</center>";
        forceStartParagraph(s);
      }
      break;
    case DocStyleChange::Small:
      if (s->enable()) m_t << "<small" << htmlAttribsToString(s->attribs()) << ">";  else m_t << "</small>";
      break;
    case DocStyleChange::Preformatted:
      if (s->enable())
      {
        forceEndParagraph(s);
        m_t << "<pre" << htmlAttribsToString(s->attribs()) << ">";
        m_insidePre=TRUE;
      }
      else
      {
        m_insidePre=FALSE;
        m_t << "</pre>";
        forceStartParagraph(s);
      }
      break;
    case DocStyleChange::Div:
      if (s->enable()) 
      {
        forceEndParagraph(s);
        m_t << "<div" << htmlAttribsToString(s->attribs()) << ">";  
      }
      else 
      {
        m_t << "</div>";
        forceStartParagraph(s);
      }
      break;
    case DocStyleChange::Span:
      if (s->enable()) m_t << "<span" << htmlAttribsToString(s->attribs()) << ">";  else m_t << "</span>";
      break;

  }
}


static void visitPreCaption(FTextStream &t, DocVerbatim *s)
{
  if (s->hasCaption())
  { 
    t << "<div class=\"caption\">" << endl;
  }
}


static void visitPostCaption(FTextStream &t, DocVerbatim *s)
{
  if (s->hasCaption())
  {
    t << "</div>" << endl;
  }
}


static void visitCaption(HtmlDocVisitor *parent, QList<DocNode> children)
{
  QListIterator<DocNode> cli(children);
  DocNode *n;
  for (cli.toFirst();(n=cli.current());++cli) n->accept(parent);
}


void HtmlDocVisitor::visit(DocVerbatim *s)
{
  if (m_hide) return;
  QCString lang = m_langExt;
  if (!s->language().isEmpty()) // explicit language setting
  {
    lang = s->language();
  }
  SrcLangExt langExt = getLanguageFromFileName(lang);
  switch(s->type())
  {
    case DocVerbatim::Code: 
      forceEndParagraph(s);
      m_t << PREFRAG_START;
      Doxygen::parserManager->getParser(lang)
                            ->parseCode(m_ci,
                                        s->context(),
                                        s->text(),
                                        langExt,
                                        s->isExample(),
                                        s->exampleFile(),
                                        0,     // fileDef
                                        -1,    // startLine
                                        -1,    // endLine
                                        FALSE, // inlineFragment
                                        0,     // memberDef
                                        TRUE,  // show line numbers
                                        m_ctx  // search context
                                       );
      m_t << PREFRAG_END;
      forceStartParagraph(s);
      break;
    case DocVerbatim::Verbatim: 
      forceEndParagraph(s);
      m_t << /*PREFRAG_START <<*/ "<pre class=\"fragment\">";
      filter(s->text());
      m_t << "</pre>" /*<< PREFRAG_END*/;
      forceStartParagraph(s);
      break;
    case DocVerbatim::HtmlOnly: 
      if (s->isBlock()) forceEndParagraph(s);
      m_t << s->text(); 
      if (s->isBlock()) forceStartParagraph(s);
      break;
    case DocVerbatim::ManOnly: 
    case DocVerbatim::LatexOnly: 
    case DocVerbatim::XmlOnly: 
    case DocVerbatim::RtfOnly:
    case DocVerbatim::DocbookOnly:
      /* nothing */ 
      break;

    case DocVerbatim::Dot:
      {
        static int dotindex = 1;
        QCString fileName(4096);

        forceEndParagraph(s);
        fileName.sprintf("%s%d%s", 
            (Config_getString(HTML_OUTPUT)+"/inline_dotgraph_").data(), 
            dotindex++,
            ".dot"
           );
        QFile file(fileName);
        if (!file.open(IO_WriteOnly))
        {
          err("Could not open file %s for writing\n",fileName.data());
        }
        else
        {
          file.writeBlock( s->text(), s->text().length() );
          file.close();

          m_t << "<div align=\"center\">" << endl;
          writeDotFile(fileName,s->relPath(),s->context());
          visitPreCaption(m_t, s);
          visitCaption(this, s->children());
          visitPostCaption(m_t, s);
          m_t << "</div>" << endl;

          if (Config_getBool(DOT_CLEANUP)) file.remove();
        }
        forceStartParagraph(s);
      }
      break;
    case DocVerbatim::Msc:
      {
        forceEndParagraph(s);

        static int mscindex = 1;
        QCString baseName(4096);

        baseName.sprintf("%s%d", 
            (Config_getString(HTML_OUTPUT)+"/inline_mscgraph_").data(), 
            mscindex++
            );
        QFile file(baseName+".msc");
        if (!file.open(IO_WriteOnly))
        {
          err("Could not open file %s.msc for writing\n",baseName.data());
        }
        else
        {
          QCString text = "msc {";
          text+=s->text();
          text+="}";

          file.writeBlock( text, text.length() );
          file.close();

          m_t << "<div align=\"center\">" << endl;
          writeMscFile(baseName+".msc",s->relPath(),s->context());
          visitPreCaption(m_t, s);
          visitCaption(this, s->children());
          visitPostCaption(m_t, s);
          m_t << "</div>" << endl;

          if (Config_getBool(DOT_CLEANUP)) file.remove();
        }
        forceStartParagraph(s);
      }
      break;
    case DocVerbatim::PlantUML:
      {
        forceEndParagraph(s);

        static QCString htmlOutput = Config_getString(HTML_OUTPUT);
        QCString baseName = writePlantUMLSource(htmlOutput,s->exampleFile(),s->text());
        m_t << "<div align=\"center\">" << endl;
        writePlantUMLFile(baseName,s->relPath(),s->context());
        visitPreCaption(m_t, s);
        visitCaption(this, s->children());
        visitPostCaption(m_t, s);
        m_t << "</div>" << endl;
        forceStartParagraph(s);
      }
      break;
  }
}

void HtmlDocVisitor::visit(DocAnchor *anc)
{
  if (m_hide) return;
  m_t << "<a class=\"anchor\" id=\"" << anc->anchor() << "\"></a>";
}

void HtmlDocVisitor::visit(DocInclude *inc)
{
  if (m_hide) return;
  SrcLangExt langExt = getLanguageFromFileName(inc->extension());
  switch(inc->type())
  {
    case DocInclude::Include: 
      forceEndParagraph(inc);
      m_t << PREFRAG_START;
      Doxygen::parserManager->getParser(inc->extension())
                            ->parseCode(m_ci,                 
                                        inc->context(),
                                        inc->text(),
                                        langExt,
                                        inc->isExample(),
                                        inc->exampleFile(),
                                        0,     // fileDef
                                        -1,    // startLine
                                        -1,    // endLine
                                        TRUE,  // inlineFragment
                                        0,     // memberDef
                                        FALSE, // show line numbers
                                        m_ctx  // search context 
                                       );
      m_t << PREFRAG_END;
      forceStartParagraph(inc);
      break;
    case DocInclude::IncWithLines:
      { 
         forceEndParagraph(inc);
         m_t << PREFRAG_START;
         QFileInfo cfi( inc->file() );
         FileDef fd( cfi.dirPath().utf8(), cfi.fileName().utf8() );
         Doxygen::parserManager->getParser(inc->extension())
                               ->parseCode(m_ci,
                                           inc->context(),
                                           inc->text(),
                                           langExt,
                                           inc->isExample(),
                                           inc->exampleFile(), 
                                           &fd,   // fileDef,
                                           -1,    // start line
                                           -1,    // end line
                                           FALSE, // inline fragment
                                           0,     // memberDef
                                           TRUE,  // show line numbers
                                           m_ctx  // search context
                                           );
         m_t << PREFRAG_END;
         forceStartParagraph(inc);
      }
      break;
    case DocInclude::DontInclude: 
      break;
    case DocInclude::HtmlInclude: 
      m_t << inc->text(); 
      break;
    case DocInclude::LatexInclude:
      break;
    case DocInclude::VerbInclude: 
      forceEndParagraph(inc);
      m_t << /*PREFRAG_START <<*/ "<pre class=\"fragment\">";
      filter(inc->text());
      m_t << "</pre>" /*<< PREFRAG_END*/;
      forceStartParagraph(inc);
      break;
    case DocInclude::Snippet:
      {
         forceEndParagraph(inc);
         m_t << PREFRAG_START;
         Doxygen::parserManager->getParser(inc->extension())
                               ->parseCode(m_ci,
                                           inc->context(),
                                           extractBlock(inc->text(),inc->blockId()),
                                           langExt,
                                           inc->isExample(),
                                           inc->exampleFile(), 
                                           0,
                                           -1,    // startLine
                                           -1,    // endLine
                                           TRUE,  // inlineFragment
                                           0,     // memberDef
                                           TRUE,  // show line number
                                           m_ctx  // search context
                                          );
         m_t << PREFRAG_END;
         forceStartParagraph(inc);
      }
      break;
  }
}

void HtmlDocVisitor::visit(DocIncOperator *op)
{
  //printf("DocIncOperator: type=%d first=%d, last=%d text=`%s'\n",
  //    op->type(),op->isFirst(),op->isLast(),op->text().data());
  if (op->isFirst()) 
  {
    if (!m_hide) m_t << PREFRAG_START;
    pushEnabled();
    m_hide=TRUE;
  }
  SrcLangExt langExt = getLanguageFromFileName(m_langExt);
  if (op->type()!=DocIncOperator::Skip) 
  {
    popEnabled();
    if (!m_hide) 
    {
      Doxygen::parserManager->getParser(m_langExt)
                            ->parseCode(
                                m_ci,
                                op->context(),
                                op->text(),
                                langExt,
                                op->isExample(),
                                op->exampleFile(),
                                0,     // fileDef
                                -1,    // startLine
                                -1,    // endLine
                                FALSE, // inline fragment
                                0,     // memberDef
                                TRUE,  // show line numbers
                                m_ctx  // search context
                               );
    }
    pushEnabled();
    m_hide=TRUE;
  }
  if (op->isLast())  
  {
    popEnabled();
    if (!m_hide) m_t << PREFRAG_END;
  }
  else
  {
    if (!m_hide) m_t << endl;
  }
}

void HtmlDocVisitor::visit(DocFormula *f)
{
  if (m_hide) return;
  bool bDisplay = !f->isInline();
  if (bDisplay) 
  {
    forceEndParagraph(f);
    m_t << "<p class=\"formulaDsp\">" << endl;
  }

  if (Config_getBool(USE_MATHJAX))
  {
    QCString text = f->text();
    bool closeInline = FALSE;
    if (!bDisplay && !text.isEmpty() && text.at(0)=='$' && 
                      text.at(text.length()-1)=='$')
    {
      closeInline=TRUE;
      text = text.mid(1,text.length()-2);
      m_t << "\\(";
    }
    m_t << convertToHtml(text);
    if (closeInline)
    {
      m_t << "\\)";
    }
  }
  else
  {
    m_t << "<img class=\"formula" 
      << (bDisplay ? "Dsp" : "Inl");
    m_t << "\" alt=\"";
    filterQuotedCdataAttr(f->text());
    m_t << "\"";
    // TODO: cache image dimensions on formula generation and give height/width
    // for faster preloading and better rendering of the page
    m_t << " src=\"" << f->relPath() << f->name() << ".png\"/>";

  }
  if (bDisplay)
  {
    m_t << endl << "</p>" << endl;
    forceStartParagraph(f);
  }
}

void HtmlDocVisitor::visit(DocIndexEntry *e)
{
  QCString anchor = convertIndexWordToAnchor(e->entry());
  if (e->member()) 
  {
    anchor.prepend(e->member()->anchor()+"_");
  }
  m_t << "<a name=\"" << anchor << "\"></a>";
  //printf("*** DocIndexEntry: word='%s' scope='%s' member='%s'\n",
  //       e->entry().data(),
  //       e->scope()  ? e->scope()->name().data()  : "<null>",
  //       e->member() ? e->member()->name().data() : "<null>"
  //      );
  Doxygen::indexList->addIndexItem(e->scope(),e->member(),anchor,e->entry());
}

void HtmlDocVisitor::visit(DocSimpleSectSep *)
{
  m_t << "</dd>" << endl;
  m_t << "<dd>" << endl;
}

void HtmlDocVisitor::visit(DocCite *cite)
{
  if (m_hide) return;
  if (!cite->file().isEmpty()) 
  {
    startLink(cite->ref(),cite->file(),cite->relPath(),cite->anchor());
  }
  else
  {
    m_t << "<b>[";
  }
  filter(cite->text());
  if (!cite->file().isEmpty()) 
  {
    endLink();
  }
  else
  {
    m_t << "]</b>";
  }
}


//--------------------------------------
// visitor functions for compound nodes
//--------------------------------------


void HtmlDocVisitor::visitPre(DocAutoList *l)
{
  //printf("DocAutoList::visitPre\n");
  if (m_hide) return;
  forceEndParagraph(l);
  if (l->isEnumList())
  {
    //
    // Do list type based on depth:
    // 1.
    //   a.
    //     i.
    //       A. 
    //         1. (repeat)...
    //
    m_t << "<ol type=\"" << types[l->depth() % NUM_HTML_LIST_TYPES] << "\">";
  }
  else
  {
    m_t << "<ul>";
  }
  if (!l->isPreformatted()) m_t << "\n";
}

void HtmlDocVisitor::visitPost(DocAutoList *l)
{
  //printf("DocAutoList::visitPost\n");
  if (m_hide) return;
  if (l->isEnumList())
  {
    m_t << "</ol>";
  }
  else
  {
    m_t << "</ul>";
  }
  if (!l->isPreformatted()) m_t << "\n";
  forceStartParagraph(l);
}

void HtmlDocVisitor::visitPre(DocAutoListItem *)
{
  if (m_hide) return;
  m_t << "<li>";
}

void HtmlDocVisitor::visitPost(DocAutoListItem *li) 
{
  if (m_hide) return;
  m_t << "</li>";
  if (!li->isPreformatted()) m_t << "\n";
}

template<class T> 
bool isFirstChildNode(T *parent, DocNode *node)
{
   return parent->children().getFirst()==node;
}

template<class T> 
bool isLastChildNode(T *parent, DocNode *node)
{
   return parent->children().getLast()==node;
}

bool isSeparatedParagraph(DocSimpleSect *parent,DocPara *par)
{
  QList<DocNode> nodes = parent->children();
  int i = nodes.findRef(par);
  if (i==-1) return FALSE;
  int count = parent->children().count();
  if (count>1 && i==0) // first node
  {
    if (nodes.at(i+1)->kind()==DocNode::Kind_SimpleSectSep)
    {
      return TRUE;
    }
  }
  else if (count>1 && i==count-1) // last node
  {
    if (nodes.at(i-1)->kind()==DocNode::Kind_SimpleSectSep)
    {
      return TRUE;
    }
  }
  else if (count>2 && i>0 && i<count-1) // intermediate node
  {
    if (nodes.at(i-1)->kind()==DocNode::Kind_SimpleSectSep &&
        nodes.at(i+1)->kind()==DocNode::Kind_SimpleSectSep)
    {
      return TRUE;
    }
  }
  return FALSE;
}

static int getParagraphContext(DocPara *p,bool &isFirst,bool &isLast)
{
  int t=0;
  isFirst=FALSE;
  isLast=FALSE;
  if (p && p->parent())
  {
    switch (p->parent()->kind()) 
    {
      case DocNode::Kind_ParBlock:
        { // hierarchy: node N -> para -> parblock -> para
          // adapt return value to kind of N
          DocNode::Kind kind = DocNode::Kind_Para;
          if ( p->parent()->parent() && p->parent()->parent()->parent() )
          {
            kind = p->parent()->parent()->parent()->kind();
          }
          isFirst=isFirstChildNode((DocParBlock*)p->parent(),p);
          isLast =isLastChildNode ((DocParBlock*)p->parent(),p);
          t=0;
          if (isFirst)
          {
            if (kind==DocNode::Kind_HtmlListItem ||
                kind==DocNode::Kind_SecRefItem)
            {
              t=1;
            }
            else if (kind==DocNode::Kind_HtmlDescData ||
                     kind==DocNode::Kind_XRefItem ||
                     kind==DocNode::Kind_SimpleSect)
            {
              t=2;
            }
            else if (kind==DocNode::Kind_HtmlCell ||
                     kind==DocNode::Kind_ParamList)
            {
              t=5;
            }
          }
          if (isLast)
          {
            if (kind==DocNode::Kind_HtmlListItem ||
                kind==DocNode::Kind_SecRefItem)
            {
              t=3;
            }
            else if (kind==DocNode::Kind_HtmlDescData ||
                     kind==DocNode::Kind_XRefItem ||
                     kind==DocNode::Kind_SimpleSect)
            {
              t=4;
            }
            else if (kind==DocNode::Kind_HtmlCell ||
                     kind==DocNode::Kind_ParamList)
            {
              t=6;
            }
          }
          break;
        }
      case DocNode::Kind_AutoListItem:
        isFirst=isFirstChildNode((DocAutoListItem*)p->parent(),p);
        isLast =isLastChildNode ((DocAutoListItem*)p->parent(),p);
        t=1; // not used
        break;
      case DocNode::Kind_SimpleListItem:
        isFirst=TRUE;
        isLast =TRUE;
        t=1; // not used
        break;
      case DocNode::Kind_ParamList:
        isFirst=TRUE;
        isLast =TRUE;
        t=1; // not used
        break;
      case DocNode::Kind_HtmlListItem:
        isFirst=isFirstChildNode((DocHtmlListItem*)p->parent(),p);
        isLast =isLastChildNode ((DocHtmlListItem*)p->parent(),p);
        if (isFirst) t=1;
        if (isLast)  t=3;
        break;
      case DocNode::Kind_SecRefItem:
        isFirst=isFirstChildNode((DocSecRefItem*)p->parent(),p);
        isLast =isLastChildNode ((DocSecRefItem*)p->parent(),p);
        if (isFirst) t=1;
        if (isLast)  t=3;
        break;
      case DocNode::Kind_HtmlDescData:
        isFirst=isFirstChildNode((DocHtmlDescData*)p->parent(),p);
        isLast =isLastChildNode ((DocHtmlDescData*)p->parent(),p);
        if (isFirst) t=2;
        if (isLast)  t=4;
        break;
      case DocNode::Kind_XRefItem:
        isFirst=isFirstChildNode((DocXRefItem*)p->parent(),p);
        isLast =isLastChildNode ((DocXRefItem*)p->parent(),p);
        if (isFirst) t=2;
        if (isLast)  t=4;
        break;
      case DocNode::Kind_SimpleSect:
        isFirst=isFirstChildNode((DocSimpleSect*)p->parent(),p);
        isLast =isLastChildNode ((DocSimpleSect*)p->parent(),p);
        if (isFirst) t=2;
        if (isLast)  t=4;
        if (isSeparatedParagraph((DocSimpleSect*)p->parent(),p))
          // if the paragraph is enclosed with separators it will
          // be included in <dd>..</dd> so avoid addition paragraph
          // markers
        {
          isFirst=isLast=TRUE;
        }
        break;
      case DocNode::Kind_HtmlCell:
        isFirst=isFirstChildNode((DocHtmlCell*)p->parent(),p);
        isLast =isLastChildNode ((DocHtmlCell*)p->parent(),p);
        if (isFirst) t=5;
        if (isLast)  t=6;
        break;
      default:
        break;
    }
    //printf("para=%p parent()->kind=%d isFirst=%d isLast=%d t=%d\n",
    //    p,p->parent()->kind(),isFirst,isLast,t);
  }
  return t;
}

void HtmlDocVisitor::visitPre(DocPara *p) 
{
  if (m_hide) return;

  //printf("DocPara::visitPre: parent of kind %d ",
  //       p->parent() ? p->parent()->kind() : -1);

  bool needsTag = FALSE;
  if (p && p->parent()) 
  {
    switch (p->parent()->kind()) 
    {
      case DocNode::Kind_Section:
      case DocNode::Kind_Internal:
      case DocNode::Kind_HtmlListItem:
      case DocNode::Kind_HtmlDescData:
      case DocNode::Kind_HtmlCell:
      case DocNode::Kind_SimpleListItem:
      case DocNode::Kind_AutoListItem:
      case DocNode::Kind_SimpleSect:
      case DocNode::Kind_XRefItem:
      case DocNode::Kind_Copy:
      case DocNode::Kind_HtmlBlockQuote:
      case DocNode::Kind_ParBlock:
        needsTag = TRUE;
        break;
      case DocNode::Kind_Root:
        needsTag = !((DocRoot*)p->parent())->singleLine();
        break;
      default:
        needsTag = FALSE;
    }
  }

  // if the first element of a paragraph is something that should be outside of
  // the paragraph (<ul>,<dl>,<table>,..) then that will already started the 
  // paragraph and we don't need to do it here
  uint nodeIndex = 0;
  if (p && nodeIndex<p->children().count())
  {
    while (nodeIndex<p->children().count() && 
           p->children().at(nodeIndex)->kind()==DocNode::Kind_WhiteSpace)
    {
      nodeIndex++;
    }
    if (nodeIndex<p->children().count())
    {
      DocNode *n = p->children().at(nodeIndex);
      if (mustBeOutsideParagraph(n))
      {
        needsTag = FALSE;
      }
    }
  }

  // check if this paragraph is the first or last child of a <li> or <dd>.
  // this allows us to mark the tag with a special class so we can
  // fix the otherwise ugly spacing.
  int t;
  static const char *contexts[7] = 
  { "",                     // 0
    " class=\"startli\"",   // 1
    " class=\"startdd\"",   // 2
    " class=\"endli\"",     // 3
    " class=\"enddd\"",     // 4
    " class=\"starttd\"",   // 5
    " class=\"endtd\""      // 6
  };
  bool isFirst;
  bool isLast;
  t = getParagraphContext(p,isFirst,isLast);
  //printf("startPara first=%d last=%d\n",isFirst,isLast);
  if (isFirst && isLast) needsTag=FALSE;

  //printf("  needsTag=%d\n",needsTag);
  // write the paragraph tag (if needed)
  if (needsTag) m_t << "<p" << contexts[t] << ">";
}

void HtmlDocVisitor::visitPost(DocPara *p)
{
  bool needsTag = FALSE;
  if (p->parent()) 
  {
    switch (p->parent()->kind()) 
    {
      case DocNode::Kind_Section:
      case DocNode::Kind_Internal:
      case DocNode::Kind_HtmlListItem:
      case DocNode::Kind_HtmlDescData:
      case DocNode::Kind_HtmlCell:
      case DocNode::Kind_SimpleListItem:
      case DocNode::Kind_AutoListItem:
      case DocNode::Kind_SimpleSect:
      case DocNode::Kind_XRefItem:
      case DocNode::Kind_Copy:
      case DocNode::Kind_HtmlBlockQuote:
      case DocNode::Kind_ParBlock:
        needsTag = TRUE;
        break;
      case DocNode::Kind_Root:
        needsTag = !((DocRoot*)p->parent())->singleLine();
        break;
      default:
        needsTag = FALSE;
    }
  }

  // if the last element of a paragraph is something that should be outside of
  // the paragraph (<ul>,<dl>,<table>) then that will already have ended the 
  // paragraph and we don't need to do it here
  int nodeIndex = p->children().count()-1;
  if (nodeIndex>=0)
  {
    while (nodeIndex>=0 && p->children().at(nodeIndex)->kind()==DocNode::Kind_WhiteSpace)
    {
      nodeIndex--;
    }
    if (nodeIndex>=0)
    {
      DocNode *n = p->children().at(nodeIndex);
      if (mustBeOutsideParagraph(n))
      {
        needsTag = FALSE;
      }
    }
  }

  bool isFirst;
  bool isLast;
  getParagraphContext(p,isFirst,isLast);
  //printf("endPara first=%d last=%d\n",isFirst,isLast);
  if (isFirst && isLast) needsTag=FALSE;

  //printf("DocPara::visitPost needsTag=%d\n",needsTag);

  if (needsTag) m_t << "</p>\n";

}

void HtmlDocVisitor::visitPre(DocRoot *)
{
}

void HtmlDocVisitor::visitPost(DocRoot *)
{
}

void HtmlDocVisitor::visitPre(DocSimpleSect *s)
{
  if (m_hide) return;
  forceEndParagraph(s);
  m_t << "<dl class=\"section " << s->typeString() << "\"><dt>";
  switch(s->type())
  {
    case DocSimpleSect::See: 
      m_t << theTranslator->trSeeAlso(); break;
    case DocSimpleSect::Return: 
      m_t << theTranslator->trReturns(); break;
    case DocSimpleSect::Author: 
      m_t << theTranslator->trAuthor(TRUE,TRUE); break;
    case DocSimpleSect::Authors: 
      m_t << theTranslator->trAuthor(TRUE,FALSE); break;
    case DocSimpleSect::Version: 
      m_t << theTranslator->trVersion(); break;
    case DocSimpleSect::Since: 
      m_t << theTranslator->trSince(); break;
    case DocSimpleSect::Date: 
      m_t << theTranslator->trDate(); break;
    case DocSimpleSect::Note: 
      m_t << theTranslator->trNote(); break;
    case DocSimpleSect::Warning:
      m_t << theTranslator->trWarning(); break;
    case DocSimpleSect::Pre:
      m_t << theTranslator->trPrecondition(); break;
    case DocSimpleSect::Post:
      m_t << theTranslator->trPostcondition(); break;
    case DocSimpleSect::Copyright:
      m_t << theTranslator->trCopyright(); break;
    case DocSimpleSect::Invar:
      m_t << theTranslator->trInvariant(); break;
    case DocSimpleSect::Remark:
      m_t << theTranslator->trRemarks(); break;
    case DocSimpleSect::Attention:
      m_t << theTranslator->trAttention(); break;
    case DocSimpleSect::User: break;
    case DocSimpleSect::Rcs: break;
    case DocSimpleSect::Unknown:  break;
  }

  // special case 1: user defined title
  if (s->type()!=DocSimpleSect::User && s->type()!=DocSimpleSect::Rcs)
  {
    m_t << "</dt><dd>";
  }
}

void HtmlDocVisitor::visitPost(DocSimpleSect *s)
{
  if (m_hide) return;
  m_t << "</dd></dl>\n";
  forceStartParagraph(s);
}

void HtmlDocVisitor::visitPre(DocTitle *)
{
}

void HtmlDocVisitor::visitPost(DocTitle *)
{
  if (m_hide) return;
  m_t << "</dt><dd>";
}

void HtmlDocVisitor::visitPre(DocSimpleList *sl)
{
  if (m_hide) return;
  forceEndParagraph(sl);
  m_t << "<ul>";
  if (!sl->isPreformatted()) m_t << "\n";

}

void HtmlDocVisitor::visitPost(DocSimpleList *sl)
{
  if (m_hide) return;
  m_t << "</ul>";
  if (!sl->isPreformatted()) m_t << "\n";
  forceStartParagraph(sl);
}

void HtmlDocVisitor::visitPre(DocSimpleListItem *)
{
  if (m_hide) return;
  m_t << "<li>";
}

void HtmlDocVisitor::visitPost(DocSimpleListItem *li) 
{
  if (m_hide) return;
  m_t << "</li>";
  if (!li->isPreformatted()) m_t << "\n";
}

void HtmlDocVisitor::visitPre(DocSection *s)
{
  if (m_hide) return;
  forceEndParagraph(s);
  m_t << "<h" << s->level() << ">";
  m_t << "<a class=\"anchor\" id=\"" << s->anchor();
  m_t << "\"></a>" << endl;
  filter(convertCharEntitiesToUTF8(s->title().data()));
  m_t << "</h" << s->level() << ">\n";
}

void HtmlDocVisitor::visitPost(DocSection *s) 
{
  forceStartParagraph(s);
}

void HtmlDocVisitor::visitPre(DocHtmlList *s)
{
  if (m_hide) return;
  forceEndParagraph(s);
  if (s->type()==DocHtmlList::Ordered) 
  {
    m_t << "<ol" << htmlAttribsToString(s->attribs()) << ">\n"; 
  }
  else 
  {
    m_t << "<ul" << htmlAttribsToString(s->attribs()) << ">\n";
  }
}

void HtmlDocVisitor::visitPost(DocHtmlList *s) 
{
  if (m_hide) return;
  if (s->type()==DocHtmlList::Ordered) 
  {
    m_t << "</ol>"; 
  }
  else
  { 
    m_t << "</ul>";
  }
  if (!s->isPreformatted()) m_t << "\n";
  forceStartParagraph(s);
}

void HtmlDocVisitor::visitPre(DocHtmlListItem *i)
{
  if (m_hide) return;
  m_t << "<li" << htmlAttribsToString(i->attribs()) << ">";
  if (!i->isPreformatted()) m_t << "\n";
}

void HtmlDocVisitor::visitPost(DocHtmlListItem *) 
{
  if (m_hide) return;
  m_t << "</li>\n";
}

void HtmlDocVisitor::visitPre(DocHtmlDescList *dl)
{
  if (m_hide) return;
  forceEndParagraph(dl);
  m_t << "<dl" << htmlAttribsToString(dl->attribs()) << ">\n";
}

void HtmlDocVisitor::visitPost(DocHtmlDescList *dl) 
{
  if (m_hide) return;
  m_t << "</dl>\n";
  forceStartParagraph(dl);
}

void HtmlDocVisitor::visitPre(DocHtmlDescTitle *dt)
{
  if (m_hide) return;
  m_t << "<dt" << htmlAttribsToString(dt->attribs()) << ">";
}

void HtmlDocVisitor::visitPost(DocHtmlDescTitle *) 
{
  if (m_hide) return;
  m_t << "</dt>\n";
}

void HtmlDocVisitor::visitPre(DocHtmlDescData *dd)
{
  if (m_hide) return;
  m_t << "<dd" << htmlAttribsToString(dd->attribs()) << ">";
}

void HtmlDocVisitor::visitPost(DocHtmlDescData *) 
{
  if (m_hide) return;
  m_t << "</dd>\n";
}

void HtmlDocVisitor::visitPre(DocHtmlTable *t)
{
  if (m_hide) return;

  forceEndParagraph(t);

  if (t->hasCaption())
  {
    m_t << "<a class=\"anchor\" id=\"" << t->caption()->anchor() << "\"></a>\n";
  }

  QString attrs = htmlAttribsToString(t->attribs());
  if (attrs.isEmpty())
  {
    m_t << "<table class=\"doxtable\">\n";
  }
  else
  {
    m_t << "<table " << htmlAttribsToString(t->attribs()) << ">\n";
  }
}

void HtmlDocVisitor::visitPost(DocHtmlTable *t) 
{
  if (m_hide) return;
  m_t << "</table>\n";
  forceStartParagraph(t);
}

void HtmlDocVisitor::visitPre(DocHtmlRow *tr)
{
  if (m_hide) return;
  m_t << "<tr" << htmlAttribsToString(tr->attribs()) << ">\n";
}

void HtmlDocVisitor::visitPost(DocHtmlRow *) 
{
  if (m_hide) return;
  m_t << "</tr>\n";
}

void HtmlDocVisitor::visitPre(DocHtmlCell *c)
{
  if (m_hide) return;
  if (c->isHeading()) 
  {
    m_t << "<th" << htmlAttribsToString(c->attribs()) << ">"; 
  }
  else 
  {
    m_t << "<td" << htmlAttribsToString(c->attribs()) << ">";
  }
}

void HtmlDocVisitor::visitPost(DocHtmlCell *c) 
{
  if (m_hide) return;
  if (c->isHeading()) m_t << "</th>"; else m_t << "</td>";
}

void HtmlDocVisitor::visitPre(DocHtmlCaption *c)
{
  if (m_hide) return;
  m_t << "<caption" << htmlAttribsToString(c->attribs()) << ">";
}

void HtmlDocVisitor::visitPost(DocHtmlCaption *) 
{
  if (m_hide) return;
  m_t << "</caption>\n";
}

void HtmlDocVisitor::visitPre(DocInternal *)
{
  if (m_hide) return;
  //forceEndParagraph(i);
  //m_t << "<p><b>" << theTranslator->trForInternalUseOnly() << "</b></p>" << endl;
}

void HtmlDocVisitor::visitPost(DocInternal *) 
{
  if (m_hide) return;
  //forceStartParagraph(i);
}

void HtmlDocVisitor::visitPre(DocHRef *href)
{
  if (m_hide) return;
  if (href->url().left(7)=="mailto:")
  {
    writeObfuscatedMailAddress(href->url().mid(7));
  }
  else
  {
    QCString url = correctURL(href->url(),href->relPath());
    m_t << "<a href=\"" << convertToXML(url)  << "\""
        << htmlAttribsToString(href->attribs()) << ">";
  }
}

void HtmlDocVisitor::visitPost(DocHRef *) 
{
  if (m_hide) return;
  m_t << "</a>";
}

void HtmlDocVisitor::visitPre(DocHtmlHeader *header)
{
  if (m_hide) return;
  forceEndParagraph(header);
  m_t << "<h" << header->level() 
      << htmlAttribsToString(header->attribs()) << ">";
}

void HtmlDocVisitor::visitPost(DocHtmlHeader *header) 
{
  if (m_hide) return;
  m_t << "</h" << header->level() << ">\n";
  forceStartParagraph(header);
}

void HtmlDocVisitor::visitPre(DocImage *img)
{
  if (img->type()==DocImage::Html)
  {
    forceEndParagraph(img);
    if (m_hide) return;
    QString baseName=img->name();
    int i;
    if ((i=baseName.findRev('/'))!=-1 || (i=baseName.findRev('\\'))!=-1)
    {
      baseName=baseName.right(baseName.length()-i-1);
    }
    m_t << "<div class=\"image\">" << endl;
    QCString url = img->url();
    if (url.isEmpty())
    {
      m_t << "<img src=\"" << img->relPath() << img->name() << "\" alt=\"" 
          << baseName << "\"" << htmlAttribsToString(img->attribs()) 
          << "/>" << endl;
    }
    else
    {
      m_t << "<img src=\"" << correctURL(url,img->relPath()) << "\" " 
          << htmlAttribsToString(img->attribs())
          << "/>" << endl;
    }
    if (img->hasCaption())
    {
      m_t << "<div class=\"caption\">" << endl;
    }
  }
  else // other format -> skip
  {
    pushEnabled();
    m_hide=TRUE;
  }
}

void HtmlDocVisitor::visitPost(DocImage *img) 
{
  if (img->type()==DocImage::Html)
  {
    if (m_hide) return;
    if (img->hasCaption())
    {
      m_t << "</div>";
    }
    m_t << "</div>" << endl;
    forceStartParagraph(img);
  }
  else // other format
  {
    popEnabled();
  }
}

void HtmlDocVisitor::visitPre(DocDotFile *df)
{
  if (m_hide) return;
  m_t << "<div class=\"dotgraph\">" << endl;
  writeDotFile(df->file(),df->relPath(),df->context());
  if (df->hasCaption())
  { 
    m_t << "<div class=\"caption\">" << endl;
  }
}

void HtmlDocVisitor::visitPost(DocDotFile *df) 
{
  if (m_hide) return;
  if (df->hasCaption())
  {
    m_t << "</div>" << endl;
  }
  m_t << "</div>" << endl;
}

void HtmlDocVisitor::visitPre(DocMscFile *df)
{
  if (m_hide) return;
  m_t << "<div class=\"mscgraph\">" << endl;
  writeMscFile(df->file(),df->relPath(),df->context());
  if (df->hasCaption())
  { 
    m_t << "<div class=\"caption\">" << endl;
  }
}
void HtmlDocVisitor::visitPost(DocMscFile *df) 
{
  if (m_hide) return;
  if (df->hasCaption())
  {
    m_t << "</div>" << endl;
  }
  m_t << "</div>" << endl;
}

void HtmlDocVisitor::visitPre(DocDiaFile *df)
{
  if (m_hide) return;
  m_t << "<div class=\"diagraph\">" << endl;
  writeDiaFile(df->file(),df->relPath(),df->context());
  if (df->hasCaption())
  {
    m_t << "<div class=\"caption\">" << endl;
  }
}
void HtmlDocVisitor::visitPost(DocDiaFile *df)
{
  if (m_hide) return;
  if (df->hasCaption())
  {
    m_t << "</div>" << endl;
  }
  m_t << "</div>" << endl;
}

void HtmlDocVisitor::visitPre(DocLink *lnk)
{
  if (m_hide) return;
  startLink(lnk->ref(),lnk->file(),lnk->relPath(),lnk->anchor());
}

void HtmlDocVisitor::visitPost(DocLink *) 
{
  if (m_hide) return;
  endLink();
}

void HtmlDocVisitor::visitPre(DocRef *ref)
{
  if (m_hide) return;
  if (!ref->file().isEmpty()) 
  {
    // when ref->isSubPage()==TRUE we use ref->file() for HTML and
    // ref->anchor() for LaTeX/RTF
    startLink(ref->ref(),ref->file(),ref->relPath(),ref->isSubPage() ? QCString() : ref->anchor());
  }
  if (!ref->hasLinkText()) filter(ref->targetTitle());
}

void HtmlDocVisitor::visitPost(DocRef *ref) 
{
  if (m_hide) return;
  if (!ref->file().isEmpty()) endLink();
  //m_t << " ";
}

void HtmlDocVisitor::visitPre(DocSecRefItem *ref)
{
  if (m_hide) return;
  QString refName=ref->file();
  if (refName.right(Doxygen::htmlFileExtension.length())!=
      QString(Doxygen::htmlFileExtension))
  {
    refName+=Doxygen::htmlFileExtension;
  }
  m_t << "<li><a href=\"" << refName << "#" << ref->anchor() << "\">";

}

void HtmlDocVisitor::visitPost(DocSecRefItem *) 
{
  if (m_hide) return;
  m_t << "</a></li>\n";
}

void HtmlDocVisitor::visitPre(DocSecRefList *s)
{
  if (m_hide) return;
  forceEndParagraph(s);
  m_t << "<div class=\"multicol\">" << endl;
  m_t << "<ul>" << endl;
}

void HtmlDocVisitor::visitPost(DocSecRefList *s) 
{
  if (m_hide) return;
  m_t << "</ul>" << endl;
  m_t << "</div>" << endl;
  forceStartParagraph(s);
}

//void HtmlDocVisitor::visitPre(DocLanguage *l)
//{
//  QString langId = Config_getEnum(OUTPUT_LANGUAGE);
//  if (l->id().lower()!=langId.lower())
//  {
//    pushEnabled();
//    m_hide = TRUE;
//  }
//}
//
//void HtmlDocVisitor::visitPost(DocLanguage *l) 
//{
//  QString langId = Config_getEnum(OUTPUT_LANGUAGE);
//  if (l->id().lower()!=langId.lower())
//  {
//    popEnabled();
//  }
//}

void HtmlDocVisitor::visitPre(DocParamSect *s)
{
  if (m_hide) return;
  forceEndParagraph(s);
  QCString className;
  QCString heading;
  switch(s->type())
  {
    case DocParamSect::Param: 
      heading=theTranslator->trParameters(); 
      className="params";
      break;
    case DocParamSect::RetVal: 
      heading=theTranslator->trReturnValues(); 
      className="retval";
      break;
    case DocParamSect::Exception: 
      heading=theTranslator->trExceptions(); 
      className="exception";
      break;
    case DocParamSect::TemplateParam: 
      heading=theTranslator->trTemplateParameters();
      className="tparams";
      break;
    default:
      ASSERT(0);
  }
  m_t << "<dl class=\"" << className << "\"><dt>";
  m_t << heading;
  m_t << "</dt><dd>" << endl;
  m_t << "  <table class=\"" << className << "\">" << endl;
}

void HtmlDocVisitor::visitPost(DocParamSect *s)
{
  if (m_hide) return;
  m_t << "  </table>" << endl;
  m_t << "  </dd>" << endl;
  m_t << "</dl>" << endl;
  forceStartParagraph(s);
}

void HtmlDocVisitor::visitPre(DocParamList *pl)
{
  //printf("DocParamList::visitPre\n");
  if (m_hide) return;
  m_t << "    <tr>";
  DocParamSect *sect = 0;
  if (pl->parent()->kind()==DocNode::Kind_ParamSect)
  {
    sect=(DocParamSect*)pl->parent();
  }
  if (sect && sect->hasInOutSpecifier())
  {
    m_t << "<td class=\"paramdir\">";
    if (pl->direction()!=DocParamSect::Unspecified)
    {
      m_t << "[";
      if (pl->direction()==DocParamSect::In)
      {
        m_t << "in";
      }
      else if (pl->direction()==DocParamSect::Out)
      {
        m_t << "out";
      }
      else if (pl->direction()==DocParamSect::InOut)
      {
        m_t << "in,out";
      }
      m_t << "]";
    }
    m_t << "</td>";
  }
  if (sect && sect->hasTypeSpecifier())
  {
    m_t << "<td class=\"paramtype\">";
    QListIterator<DocNode> li(pl->paramTypes());
    DocNode *type;
    bool first=TRUE;
    for (li.toFirst();(type=li.current());++li)
    {
      if (!first) m_t << "&#160;|&#160;"; else first=FALSE;
      if (type->kind()==DocNode::Kind_Word)
      {
        visit((DocWord*)type); 
      }
      else if (type->kind()==DocNode::Kind_LinkedWord)
      {
        visit((DocLinkedWord*)type); 
      }
    }
    m_t << "</td>";
  }
  m_t << "<td class=\"paramname\">";
  //QStrListIterator li(pl->parameters());
  //const char *s;
  QListIterator<DocNode> li(pl->parameters());
  DocNode *param;
  bool first=TRUE;
  for (li.toFirst();(param=li.current());++li)
  {
    if (!first) m_t << ","; else first=FALSE;
    if (param->kind()==DocNode::Kind_Word)
    {
      visit((DocWord*)param); 
    }
    else if (param->kind()==DocNode::Kind_LinkedWord)
    {
      visit((DocLinkedWord*)param); 
    }
  }
  m_t << "</td><td>";
}

void HtmlDocVisitor::visitPost(DocParamList *)
{
  //printf("DocParamList::visitPost\n");
  if (m_hide) return;
  m_t << "</td></tr>" << endl;
}

void HtmlDocVisitor::visitPre(DocXRefItem *x)
{
  if (m_hide) return;
  if (x->title().isEmpty()) return;

  forceEndParagraph(x);
  bool anonymousEnum = x->file()=="@";
  if (!anonymousEnum)
  {
    m_t << "<dl class=\"" << x->key() << "\"><dt><b><a class=\"el\" href=\"" 
        << x->relPath() << x->file() << Doxygen::htmlFileExtension 
        << "#" << x->anchor() << "\">";
  }
  else 
  {
    m_t << "<dl class=\"" << x->key() << "\"><dt><b>";
  }
  filter(x->title());
  m_t << ":";
  if (!anonymousEnum) m_t << "</a>";
  m_t << "</b></dt><dd>";
}

void HtmlDocVisitor::visitPost(DocXRefItem *x)
{
  if (m_hide) return;
  if (x->title().isEmpty()) return;
  m_t << "</dd></dl>" << endl;
  forceStartParagraph(x);
}

void HtmlDocVisitor::visitPre(DocInternalRef *ref)
{
  if (m_hide) return;
  startLink(0,ref->file(),ref->relPath(),ref->anchor());
}

void HtmlDocVisitor::visitPost(DocInternalRef *) 
{
  if (m_hide) return;
  endLink();
  m_t << " ";
}

void HtmlDocVisitor::visitPre(DocCopy *)
{
}

void HtmlDocVisitor::visitPost(DocCopy *)
{
}

void HtmlDocVisitor::visitPre(DocText *)
{
}

void HtmlDocVisitor::visitPost(DocText *)
{
}

void HtmlDocVisitor::visitPre(DocHtmlBlockQuote *b)
{
  if (m_hide) return;
  forceEndParagraph(b);
  QString attrs = htmlAttribsToString(b->attribs());
  if (attrs.isEmpty())
  {
    m_t << "<blockquote class=\"doxtable\">\n";
  }
  else
  {
    m_t << "<blockquote " << htmlAttribsToString(b->attribs()) << ">\n";
  }
}

void HtmlDocVisitor::visitPost(DocHtmlBlockQuote *b)
{
  if (m_hide) return;
  m_t << "</blockquote>" << endl;
  forceStartParagraph(b);
}

void HtmlDocVisitor::visitPre(DocVhdlFlow *vf)
{
  if (m_hide) return;
  if (VhdlDocGen::getFlowMember()) // use VHDL flow chart creator
  {
    forceEndParagraph(vf);
    QCString fname=FlowChart::convertNameToFileName();
    m_t << "<p>";
    m_t << "flowchart: " ; // TODO: translate me
    m_t << "<a href=\"";
    m_t << fname.data(); 
    m_t << ".svg\">";
    m_t << VhdlDocGen::getFlowMember()->name().data();
    m_t << "</a>";
    if (vf->hasCaption())
    {
      m_t << "<br />";
    }
  }
}

void HtmlDocVisitor::visitPost(DocVhdlFlow *vf)
{
  if (m_hide) return;
  if (VhdlDocGen::getFlowMember()) // use VHDL flow chart creator
  {
    m_t << "</p>";
    forceStartParagraph(vf);
  }
}

void HtmlDocVisitor::visitPre(DocParBlock *)
{
  if (m_hide) return;
}

void HtmlDocVisitor::visitPost(DocParBlock *)
{
  if (m_hide) return;
}



void HtmlDocVisitor::filter(const char *str)
{ 
  if (str==0) return;
  const char *p=str;
  char c;
  while (*p)
  {
    c=*p++;
    switch(c)
    {
      case '<':  m_t << "&lt;"; break;
      case '>':  m_t << "&gt;"; break;
      case '&':  m_t << "&amp;"; break;
      default:   m_t << c;
    }
  }
}

/// Escape basic entities to produce a valid CDATA attribute value,
/// assume that the outer quoting will be using the double quote &quot;
void HtmlDocVisitor::filterQuotedCdataAttr(const char* str)
{
  if (str==0) return;
  const char *p=str;
  char c;
  while (*p)
  {
    c=*p++;
    switch(c)
    {
      case '&':  m_t << "&amp;"; break;
      case '"':  m_t << "&quot;"; break;
      case '<':  m_t << "&lt;"; break;
      case '>':  m_t << "&gt;"; break;
      default:   m_t << c;
    }
  }
}

void HtmlDocVisitor::startLink(const QCString &ref,const QCString &file,
                               const QCString &relPath,const QCString &anchor,
                               const QCString &tooltip)
{
  //printf("HtmlDocVisitor: file=%s anchor=%s\n",file.data(),anchor.data());
  if (!ref.isEmpty()) // link to entity imported via tag file
  {
    m_t << "<a class=\"elRef\" ";
    m_t << externalLinkTarget() << externalRef(relPath,ref,FALSE);
  }
  else // local link
  {
    m_t << "<a class=\"el\" ";
  }
  m_t << "href=\"";
  m_t << externalRef(relPath,ref,TRUE);
  if (!file.isEmpty()) m_t << file << Doxygen::htmlFileExtension;
  if (!anchor.isEmpty()) m_t << "#" << anchor;
  m_t << "\"";
  if (!tooltip.isEmpty()) m_t << " title=\"" << convertToHtml(tooltip) << "\"";
  m_t << ">";
}

void HtmlDocVisitor::endLink()
{
  m_t << "</a>";
}

void HtmlDocVisitor::pushEnabled()
{
  m_enabled.push(new bool(m_hide));
}

void HtmlDocVisitor::popEnabled()
{
  bool *v=m_enabled.pop();
  ASSERT(v!=0);
  m_hide = *v;
  delete v;
}

void HtmlDocVisitor::writeDotFile(const QCString &fn,const QCString &relPath,
                                  const QCString &context)
{
  QCString baseName=fn;
  int i;
  if ((i=baseName.findRev('/'))!=-1)
  {
    baseName=baseName.right(baseName.length()-i-1);
  }
  if ((i=baseName.find('.'))!=-1) // strip extension
  {
    baseName=baseName.left(i);
  }
  baseName.prepend("dot_");
  QCString outDir = Config_getString(HTML_OUTPUT);
  writeDotGraphFromFile(fn,outDir,baseName,GOF_BITMAP);
  writeDotImageMapFromFile(m_t,fn,outDir,relPath,baseName,context);
}

void HtmlDocVisitor::writeMscFile(const QCString &fileName,
                                  const QCString &relPath,
                                  const QCString &context)
{
  QCString baseName=fileName;
  int i;
  if ((i=baseName.findRev('/'))!=-1) // strip path
  {
    baseName=baseName.right(baseName.length()-i-1);
  }
  if ((i=baseName.find('.'))!=-1) // strip extension
  {
    baseName=baseName.left(i);
  }
  baseName.prepend("msc_");
  QCString outDir = Config_getString(HTML_OUTPUT);
  QCString imgExt = getDotImageExtension();
  MscOutputFormat mscFormat = MSC_BITMAP;
  if ("svg" == imgExt)
    mscFormat = MSC_SVG;
  writeMscGraphFromFile(fileName,outDir,baseName,mscFormat);
  writeMscImageMapFromFile(m_t,fileName,outDir,relPath,baseName,context,mscFormat);
}

void HtmlDocVisitor::writeDiaFile(const QCString &fileName,
                                  const QCString &relPath,
                                  const QCString &)
{
  QCString baseName=fileName;
  int i;
  if ((i=baseName.findRev('/'))!=-1) // strip path
  {
    baseName=baseName.right(baseName.length()-i-1);
  }
  if ((i=baseName.find('.'))!=-1) // strip extension
  {
    baseName=baseName.left(i);
  }
  baseName.prepend("dia_");
  QCString outDir = Config_getString(HTML_OUTPUT);
  writeDiaGraphFromFile(fileName,outDir,baseName,DIA_BITMAP);

  m_t << "<img src=\"" << relPath << baseName << ".png" << "\" />" << endl;
}

void HtmlDocVisitor::writePlantUMLFile(const QCString &fileName,
                                       const QCString &relPath,
                                       const QCString &)
{
  QCString baseName=fileName;
  int i;
  if ((i=baseName.findRev('/'))!=-1) // strip path
  {
    baseName=baseName.right(baseName.length()-i-1);
  }
  if ((i=baseName.findRev('.'))!=-1) // strip extension
  {
    baseName=baseName.left(i);
  }
  static QCString outDir = Config_getString(HTML_OUTPUT);
  QCString imgExt = getDotImageExtension();
  if (imgExt=="svg")
  {
    generatePlantUMLOutput(fileName,outDir,PUML_SVG);
    //m_t << "<iframe scrolling=\"no\" frameborder=\"0\" src=\"" << relPath << baseName << ".svg" << "\" />" << endl;
    //m_t << "<p><b>This browser is not able to show SVG: try Firefox, Chrome, Safari, or Opera instead.</b></p>";
    //m_t << "</iframe>" << endl;
    m_t << "<object type=\"image/svg+xml\" data=\"" << relPath << baseName << ".svg\"></object>" << endl;
  }
  else
  {
    generatePlantUMLOutput(fileName,outDir,PUML_BITMAP);
    m_t << "<img src=\"" << relPath << baseName << ".png" << "\" />" << endl;
  }
}

/** Returns TRUE if the child nodes in paragraph \a para until \a nodeIndex
    contain a style change node that is still active and that style change is one that
    must be located outside of a paragraph, i.e. it is a center, div, or pre tag.
    See also bug746162.
 */
static bool insideStyleChangeThatIsOutsideParagraph(DocPara *para,int nodeIndex)
{
  //printf("insideStyleChangeThatIsOutputParagraph(index=%d)\n",nodeIndex);
  int styleMask=0;
  bool styleOutsideParagraph=FALSE;
  while (nodeIndex>=0 && !styleOutsideParagraph)
  {
    DocNode *n = para->children().at(nodeIndex);
    if (n->kind()==DocNode::Kind_StyleChange)
    {
      DocStyleChange *sc = (DocStyleChange*)n;
      if (!sc->enable()) // remember styles that has been closed already
      {
        styleMask|=(int)sc->style();
      }
      bool paraStyle = sc->style()==DocStyleChange::Center ||
                       sc->style()==DocStyleChange::Div    ||
                       sc->style()==DocStyleChange::Preformatted;
      //printf("Found style change %s enabled=%d\n",sc->styleString(),sc->enable());
      if (sc->enable() && (styleMask&(int)sc->style())==0 && // style change that is still active
          paraStyle
         )
      {
        styleOutsideParagraph=TRUE;
      }
    }
    nodeIndex--;
  }
  return styleOutsideParagraph;
}

/** Used for items found inside a paragraph, which due to XHTML restrictions
 *  have to be outside of the paragraph. This method will forcefully end
 *  the current paragraph and forceStartParagraph() will restart it.
 */
void HtmlDocVisitor::forceEndParagraph(DocNode *n)
{
  //printf("forceEndParagraph(%p) %d\n",n,n->kind());
  if (n->parent() && n->parent()->kind()==DocNode::Kind_Para)
  {
    DocPara *para = (DocPara*)n->parent();
    int nodeIndex = para->children().findRef(n);
    nodeIndex--;
    if (nodeIndex<0) return; // first node
    while (nodeIndex>=0 &&
           para->children().at(nodeIndex)->kind()==DocNode::Kind_WhiteSpace
          )
    {
      nodeIndex--;
    }
    if (nodeIndex>=0)
    {
      DocNode *n = para->children().at(nodeIndex);
      //printf("n=%p kind=%d outside=%d\n",n,n->kind(),mustBeOutsideParagraph(n));
      if (mustBeOutsideParagraph(n)) return;
    }
    nodeIndex--;
    bool styleOutsideParagraph=insideStyleChangeThatIsOutsideParagraph(para,nodeIndex);
    bool isFirst;
    bool isLast;
    getParagraphContext(para,isFirst,isLast);
    //printf("forceEnd first=%d last=%d styleOutsideParagraph=%d\n",isFirst,isLast,styleOutsideParagraph);
    if (isFirst && isLast) return;
    if (styleOutsideParagraph) return;

    m_t << "</p>";
  }
}

/** Used for items found inside a paragraph, which due to XHTML restrictions
 *  have to be outside of the paragraph. This method will forcefully start
 *  the paragraph, that was previously ended by forceEndParagraph().
 */
void HtmlDocVisitor::forceStartParagraph(DocNode *n)
{
  //printf("forceStartParagraph(%p) %d\n",n,n->kind());
  if (n->parent() && n->parent()->kind()==DocNode::Kind_Para) // if we are inside a paragraph
  {
    DocPara *para = (DocPara*)n->parent();
    int nodeIndex = para->children().findRef(n);
    int numNodes  = para->children().count();
    bool styleOutsideParagraph=insideStyleChangeThatIsOutsideParagraph(para,nodeIndex);
    if (styleOutsideParagraph) return;
    nodeIndex++;
    if (nodeIndex==numNodes) return; // last node
    while (nodeIndex<numNodes &&
           para->children().at(nodeIndex)->kind()==DocNode::Kind_WhiteSpace
          )
    {
      nodeIndex++;
    }
    if (nodeIndex<numNodes)
    {
      DocNode *n = para->children().at(nodeIndex);
      if (mustBeOutsideParagraph(n)) return;
    }
    else
    {
      return; // only whitespace at the end!
    }

    bool isFirst;
    bool isLast;
    getParagraphContext(para,isFirst,isLast);
    //printf("forceStart first=%d last=%d\n",isFirst,isLast);
    if (isFirst && isLast) return;

    m_t << "<p>";
  }
}

