// FIXMEs: 
// 1) BNGL (BNG2.pl) requires whitespace to be precisely defined,
//    need to update the parser and tests
// 2) Parentheses are not required for molecule types and molecule instances


// for top of bngl_parser.hpp
%code requires {      
#include "bng/ast.h"
}

// for top of bngl_parser.cpp
%code top {      
#include "bng/ast.h"
}

// for bngl_parser.hpp
%code provides {
    
namespace BNG {
  void create_parser_context();
  ParserContext* get_parser_context();
  void delete_parser_context();
}

}

// for bngl_parser.cpp
%{
  #include <cstdio>
  #include <cstdarg>
  #include <string>
  #include "bng/parser_utils.h"
    
  // Declare stuff from Flex that Bison needs to know about:
  extern int bngllex();
  
  void bnglerror(char const *s);

  // global context used during parsing
  BNG::ParserContext* g_ctx;
%}


%require "3.0"

// add debug output code to generated parser 
// one also needs to set bngldebug to 1
// extern int bngldebug;
// bngldebug = 1;
//%debug

// write out a header file containing the token defines 
%defines

%error-verbose

// set up function name prefixes and output file name 
%define api.prefix {bngl}

// extend yylval (bngllval) with the possibility to store line)
%locations

// One shift-reduce conflict is expected, the reason is that 
// the beginning of the molecule types section is resolved
// only after the first molecule type declaration is parsed as shown here.
// So we need to use GLR parser that is able to do arbitrary lookahead.
//
// begin molecule types
//   A(a)
// end molecule types
// begin reaction rules
//   A(a) +|.
// end reaction rules
//
%glr-parser
%expect 6

%union {
  const char* str;
  double dbl;
  long long llong;
  bool boolean;
  BNG::ASTExprNode* expr_node;
  BNG::ASTListNode* list_node;
  BNG::ASTStrNode* str_node;
  BNG::ASTComponentNode* component_node;
  BNG::ASTMolNode* mol_node;
  BNG::ASTCplxNode* cplx_node;
}

%token TOK_BEGIN "begin"
%token TOK_END "end"
%token TOK_MODEL "model"
%token TOK_PARAMETERS "parameters"
%token TOK_MOLECULE "molecule"
%token TOK_TYPES "types"
%token TOK_COMPARTMENTS "compartments"
%token TOK_REACTION "reaction"
%token TOK_RULES "rules"
%token TOK_SEED "seed"
%token TOK_SPECIES "species"
%token TOK_OBSERVABLES "observables"
%token TOK_ACTIONS "actions"

// special token to switch parser to mode where it parses a single complex
%token TOK_SINGLE_CPLX "!CPLX"

%token <str> TOK_ID "identifier"
%token <dbl> TOK_DBL "floating point constant"
%token <llong> TOK_LLONG "integer constant"
%token <str> TOK_STR "string literal"

%token TOK_ARROW_RIGHT "->"
%token TOK_ARROW_BIDIR "<->"
%token TOK_ARG_ASSIGN  "=>"

%type <expr_node> expr
%type <str_node> bond_maybe_empty
%type <str_node> component_state 
%type <component_node> component
%type <list_node> component_state_list_maybe_empty
%type <list_node> component_state_list
%type <list_node> component_list_maybe_empty
%type <list_node> component_list
%type <mol_node> mol
%type <str_node> molecule_compartment
%type <list_node> molecule_list_maybe_empty
%type <list_node> mol_list
%type <str_node> rxn_rule_name_maybe_empty
%type <list_node> rxn_rule_side_or_zero
%type <list_node> rxn_rule_side
%type <list_node> rates
%type <boolean> rxn_rule_direction
%type <list_node> cplx_list
%type <cplx_node> cplx
%type <cplx_node> cplx_no_compartment
%type <str_node> cplx_compartment

// operator associativities and precendences
// unary minus has really lower precendence than power 
%left '+' '-'
%left '*' '/'
%left UNARYPLUS UNARYMINUS
%left '^'   

%%

// TODO: error recovery 
start_bngl:
      model_sections action_section  // default mode to parse BNGL file
      
    | TOK_SINGLE_CPLX cplx {  // single complex to be parsed, prefixed by a unique string
    	g_ctx->single_cplx = $2;
    }
    // empty file
    | 
;

model_sections:
	  section_list 
	| TOK_BEGIN TOK_MODEL section_list TOK_END TOK_MODEL

section_list:
      section_list section
    | section
;
 
action_section:
      action_call_list_maybe_empty
    | TOK_BEGIN TOK_ACTIONS action_call_list_maybe_empty TOK_END TOK_ACTIONS
     
section:
      TOK_BEGIN TOK_PARAMETERS parameter_list_maybe_empty TOK_END TOK_PARAMETERS
    | TOK_BEGIN TOK_MOLECULE TOK_TYPES molecule_list_maybe_empty TOK_END TOK_MOLECULE TOK_TYPES {
        g_ctx->symtab.insert_molecule_declarations($4, g_ctx);
      }
    | TOK_BEGIN TOK_COMPARTMENTS compartment_list_maybe_empty TOK_END TOK_COMPARTMENTS    
    | TOK_BEGIN TOK_REACTION TOK_RULES rxn_rule_list_maybe_empty TOK_END TOK_REACTION TOK_RULES 
    | TOK_BEGIN TOK_SEED TOK_SPECIES seed_species_list_maybe_empty TOK_END TOK_SEED TOK_SPECIES
    | TOK_BEGIN TOK_SPECIES seed_species_list_maybe_empty TOK_END TOK_SPECIES
    | TOK_BEGIN TOK_OBSERVABLES observables_list_maybe_empty TOK_END TOK_OBSERVABLES
;

// ---------------- parameters ------------------- 
parameter_list_maybe_empty:
      parameter_list
    | /* empty */
;

parameter_list:
      parameter_list parameter
    | parameter
;
      
parameter:
      TOK_ID expr {
        g_ctx->symtab.insert($1, $2, g_ctx);
      }
    | TOK_ID '=' expr {
        g_ctx->symtab.insert($1, $3, g_ctx);
      }
;
      
// ---------------- molecules -------------------     
molecule_list_maybe_empty:
      mol_list
    | /* empty */ {
        $$ = g_ctx->new_list_node();
      }
;

// left recursion is preferred 
mol_list:
      mol_list mol {
        $1->append($2);
        $$ = $1;
      }
    | mol {
        $$ = g_ctx->new_list_node()->append($1);
      }
;

// fully general specification, might contain information on bonds, checked later in semantic checks 
mol:
      TOK_ID '(' component_list_maybe_empty ')' molecule_compartment {
        $$ = g_ctx->new_molecule_node($1, $3, $5, @1);    
      }
    | TOK_ID molecule_compartment {
        // no components neither parentheses
        $$ = g_ctx->new_molecule_node($1, g_ctx->new_list_node(), $2, @1);    
      }
;

molecule_compartment:
      '@' TOK_ID {
        $$ = g_ctx->new_str_node($2, @2);
      }
    | /* empty */ {
        $$ = nullptr;
      }
;

component_list_maybe_empty:
      component_list
    | /* empty */ {
        $$ = g_ctx->new_list_node();
      }
;

component_list:
      component_list ',' component {
        $1->append($3);
        $$ = $1;
      }
    | component {
        $$ = g_ctx->new_list_node()->append($1);
      }
;

component:
      TOK_ID component_state_list_maybe_empty bond_maybe_empty {
        $$ = g_ctx->new_component_node($1, $2, $3, @1);
      }
; 

component_state_list_maybe_empty:
      component_state_list
    | /* empty */ {
        $$ = g_ctx->new_list_node();
      }
;
    
component_state_list:
      component_state_list component_state {
        $1->append($2);
        $$ = $1;
      }
    | component_state {
        $$ = g_ctx->new_list_node()->append($1);
      }
;

component_state:
      '~' TOK_ID {
        $$ = g_ctx->new_str_node($2, @2);
      }
    | '~' TOK_LLONG {
        $$ = g_ctx->new_str_node($2, @2);
      }
;

bond_maybe_empty:
      '!' TOK_LLONG {
        $$ = g_ctx->new_str_node($2, @2);
      }
    | '!' '+' {
        $$ = g_ctx->new_str_node(BNG::BOND_STR_BOUND, @2);
      }
    | '!' '?' {
        $$ = g_ctx->new_str_node(BNG::BOND_STR_ANY, @2);
      }      
    | /* empty */ {
        $$ = g_ctx->new_empty_str_node();
    }
;
    
// ---------------- compartments -------------------    
compartment_list_maybe_empty:
	  compartment_list
    | /* empty */ 
;
    
compartment_list:
      compartment_list compartment_decl
	| compartment_decl
;

compartment_decl:
      TOK_ID TOK_LLONG expr TOK_ID {
        g_ctx->add_compartment(
            g_ctx->new_compartment_node($1, $2, $3, $4, @1)
        );
    }
    | TOK_ID TOK_LLONG expr {
        g_ctx->add_compartment(
            g_ctx->new_compartment_node($1, $2, $3, "", @1)
        );
    }
;    
	    
// ---------------- rxn_rules ------------------- 
rxn_rule_list_maybe_empty:
      rxn_rule_list
    | /* empty */ 
;

rxn_rule_list:
      rxn_rule_list rxn_rule 
    | rxn_rule 
;

rxn_rule:
      rxn_rule_name_maybe_empty rxn_rule_side rxn_rule_direction rxn_rule_side_or_zero rates {
         
        BNG::ASTRxnRuleNode* n = g_ctx->new_rxn_rule_node($1, $2, $3, $4, $5);
        g_ctx->add_rxn_rule(n);
      }
;

rxn_rule_name_maybe_empty:
      TOK_ID ':' {
        $$ = g_ctx->new_str_node($1, @1);
      }
    | /* empty */ {
        $$ = g_ctx->new_empty_str_node();
    }
;    

rxn_rule_side_or_zero:
      rxn_rule_side
    | TOK_LLONG {
        if ($1 != 0) {
          bnglerror("Unexpected constant on the right-hand side of a reaction, only '0' is accepted.");
        }
        // 0 is the same as molecule name Null and Thrash, we will create a complex with a single molecule 
        $$ = g_ctx->new_list_node()->append(
        	g_ctx->new_molecule_node("Null", g_ctx->new_list_node(), nullptr, @1)
       	);
      }
;

rxn_rule_side:
      rxn_rule_side '+' cplx {
        $1->append($3);
        $$ = $1;
      }
    | cplx {
        $$ = g_ctx->new_list_node()->append($1);
      }
;

rxn_rule_direction:
      TOK_ARROW_RIGHT {
        $$ = false;
      }
    | TOK_ARROW_BIDIR {
        $$ = true;
      }
;

rates:
      rates ',' expr {
         $1->append($3);
         $$ = $1; 
      }
    | expr {
        $$ = g_ctx->new_list_node()->append($1);
      }
;

// ---------------- seed species ---------------------

seed_species_list_maybe_empty:
    seed_species_list
    | /* empty */ 
;

seed_species_list:
      seed_species_list seed_species_item 
    | seed_species_item
;

seed_species_item:
      cplx expr {

        BNG::ASTSeedSpeciesNode* n = g_ctx->new_seed_species_node($1, $2); 
        g_ctx->add_seed_species(n);
      }
;


// similar to rxn_rule_side only contains one complex
cplx: 
      cplx_compartment cplx_no_compartment {
        $2->compartment = $1;
        $$ = $2;
      }

cplx_compartment:
      '@' TOK_ID ':' {
        $$ = g_ctx->new_str_node($2, @2);
      }
    | /* empty */ {
        $$ = nullptr;
      }
;

cplx_no_compartment:
      cplx_no_compartment '.' mol {
        $1->append($3);
        $$ = $1;
      }
    | mol {
        $$ = g_ctx->new_cplx_node($1);
      }
;
      
// ---------------- observables ---------------------
      
observables_list_maybe_empty:
      observables_list
    | /* empty */ 
;

observables_list:
      observables_list observables_item 
    | observables_item
;
      
observables_item:
      TOK_ID TOK_ID cplx_list {
        BNG::ASTObservableNode* n = g_ctx->new_observable_node($1, $2, $3, @1); 
        g_ctx->add_observable(n);
      }
;

cplx_list:
      cplx_list ',' cplx {
        $1->append($3);
      }
    | cplx {
    	$$ = g_ctx->new_list_node()->append($1);
      }
      
// ---------------- action calls ------------------
// ignored

action_call_list_maybe_empty:
      action_call_list
    | /* empty */
;

action_call_list:
      action_call_list action_call
    | action_call
;

action_call:
      TOK_ID '(' arg_list_maybe_empty ')' maybe_semicolon

arg_list_maybe_empty:
      '{' action_arg_list '}'
    | function_arg_list
    | /* empty */
;

action_arg_list:
      action_arg_list ',' action_arg
    | action_arg
;

action_arg:
      id_incl_keywords TOK_ARG_ASSIGN expr_or_str
;

function_arg_list:
      function_arg_list ',' function_arg
    | function_arg
;

function_arg:
      expr_or_str
;

id_incl_keywords:
	  TOK_ID
	| TOK_BEGIN
	| TOK_END
	| TOK_MODEL
	| TOK_PARAMETERS
	| TOK_MOLECULE
	| TOK_TYPES
	| TOK_COMPARTMENTS
	| TOK_REACTION
	| TOK_RULES
	| TOK_SEED
	| TOK_SPECIES
	| TOK_OBSERVABLES
	| TOK_ACTIONS
	;
 
maybe_semicolon:
	   ';'
	 | /* empty */
	 ;
 
expr_or_str:
      expr 
    | TOK_STR      
;
      
// ---------------- expressions --------------------- 
expr:
      TOK_ID                        { $$ = g_ctx->new_id_node($1, @1); } 
    | TOK_DBL                       { $$ = g_ctx->new_dbl_node($1, @1); } 
    | TOK_LLONG                     { $$ = g_ctx->new_llong_node($1, @1); }
    | '(' expr ')'                  { $$ = $2; } 
    | expr '+' expr                 { $$ = g_ctx->new_expr_node($1, BNG::ExprType::Add, $3, @2); }
    | expr '-' expr                 { $$ = g_ctx->new_expr_node($1, BNG::ExprType::Sub, $3, @2); }
    | expr '*' expr                 { $$ = g_ctx->new_expr_node($1, BNG::ExprType::Mul, $3, @2); }
    | expr '/' expr                 { $$ = g_ctx->new_expr_node($1, BNG::ExprType::Div, $3, @2); }
    | expr '^' expr                 { $$ = g_ctx->new_expr_node($1, BNG::ExprType::Pow, $3, @2); }
    | '+' expr %prec UNARYPLUS      { $$ = g_ctx->new_expr_node($2, BNG::ExprType::UnaryPlus, nullptr, @1); }
    | '-' expr %prec UNARYMINUS     { $$ = g_ctx->new_expr_node($2, BNG::ExprType::UnaryMinus, nullptr, @1); }
;
    
%%

void bnglerror(char const *s) {
  BNG::errs_loc() << s << "\n";
  g_ctx->inc_error_count();
}

namespace BNG {
void create_parser_context() {
  assert(g_ctx == nullptr);
  g_ctx = new ParserContext();
}

ParserContext* get_parser_context() {
  return g_ctx;
}

void delete_parser_context() {
  delete g_ctx;
  g_ctx = nullptr;
}

}