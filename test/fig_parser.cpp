/*
 * Rules for a FigParser
 *
 * You must declare this type in fig_parser.h by expanding the Peg<> template
 */
#include	<fig_parser.h>

const char*	TOP_captures[] = { "bom", "definition", 0 };
const char*	definition_captures[] = { "node", 0 };
const char*	factType_captures[] = { "predicate", "typename", 0 };
const char*	alternatePredicate_captures[] = { "predicate", "roleNumber", 0 };
const char*	roleNaming_captures[] = { "predicateRole", "roleName", 0 };
const char*	mandatory_captures[] = { "typename", "predicateRole", 0 };
const char*	unique_captures[] = { "predicateRole", 0 };
const char*	simpleIdentification_captures[] = { "typename", "predicateRole", 0 };
const char*	externalUnique_captures[] = { "predicateRole", 0 };
const char*	externalIdentification_captures[] = { "typename", "predicateRole", 0 };
const char*	frequency_captures[] = { "e", "predicateRole", "frequencyRanges", 0 };
const char*	frequencyRanges_captures[] = { "frequencyRange", 0 };
const char*	frequencyRange_captures[] = { "low", "high", 0 };
const char*	subtype_captures[] = { "typename", 0 };
const char*	subtypeConstrained_captures[] = { "e", "super", "typename", 0 };
const char*	subset_captures[] = { "rolePairs", 0 };
const char*	exclusive_captures[] = { "rolePairs", 0 };
const char*	equality_captures[] = { "rolePairs", 0 };
const char*	rolePairs_captures[] = { "rolePair", 0 };
const char*	rolePair_captures[] = { "predicateRole", 0 };
const char*	typeCardinality_captures[] = { "typename", "cardinalityRange", 0 };
const char*	roleCardinality_captures[] = { "predicateRole", "cardinalityRange", 0 };
const char*	cardinalityRange_captures[] = { "low", "high", 0 };
const char*	objectifies_captures[] = { "typename", "predicate", 0 };
const char*	linkFactType_captures[] = { "predicate", "predicateRole", 0 };
const char*	valuesOf_captures[] = { "target", "range", 0 };
const char*	comparison_captures[] = { "comparisonOperator", "predicateRole", 0 };
const char*	comparisonOperator_captures[] = { "op", 0 };
const char*	ringConstraint_captures[] = { "ringConstraintType", "predicateRole", 0 };
const char*	subTypeRule_captures[] = { "semi", "typename", "path", 0 };
const char*	factTypeRule_captures[] = { "semi", "predicate", "path", 0 };
const char*	joinPath_captures[] = { "predicate", "rolePairs", 0 };
const char*	path_captures[] = { "pathDisjunction", 0 };
const char*	pathDisjunction_captures[] = { "pathConjunction", 0 };
const char*	pathConjunction_captures[] = { "pathException", 0 };
const char*	pathException_captures[] = { "pathSimple", 0 };
const char*	pathSimple_captures[] = { "simple", 0 };
const char*	roleTraversal_captures[] = { "startRole", "predicateRole", "path", 0 };
const char*	unaryPredicate_captures[] = { "predicate", 0 };
const char*	typePredicate_captures[] = { "typename", 0 };
const char*	valueSetPredicate_captures[] = { "literal", 0 };
const char*	variableComparison_captures[] = { "comparisonOperator", "term", 0 };
const char*	variableBinding_captures[] = { "variable", 0 };
const char*	subexpression_captures[] = { "path", 0 };
const char*	term_captures[] = { "term", 0 };
const char*	functionCall_captures[] = { "id", "term", 0 };
const char*	variable_captures[] = { "id", 0 };
const char*	roleName_captures[] = { "id", 0 };
const char*	predicate_captures[] = { "t", 0 };
const char*	predicateRole_captures[] = { "predicate", "roleNumber", 0 };
const char*	roleNumber_captures[] = { "naturalNumber", 0 };
const char*	range_captures[] = { "range", 0 };
const char*	numeric_range_captures[] = { "low", "high", 0 };
const char*	string_range_captures[] = { "low", "high", 0 };
const char*	number_captures[] = { "number", 0 };
const char*	typename_captures[] = { "id", 0 };

template<>FigParser::Rule	FigParser::rules[] =
{
	{ "TOP",
	  "?(<BOM>:bom)*<definition>",
	  TOP_captures
	},
	{ "BOM",
	  "\\uFEFF",
	  0
	},
	{ "definition",
	  "<s>(|<factType>:node:|<valuesOf>:node:|<alternatePredicate>:node:|<roleNaming>:node:|<mandatory>:node:|<unique>:node:|<simpleIdentification>:node:|<externalUnique>:node:|<externalIdentification>:node:|<frequency>:node:|<subtype>:node:|<subtypeConstrained>:node:|<subset>:node:|<exclusive>:node:|<equality>:node:|<typeCardinality>:node:|<roleCardinality>:node:|<objectifies>:node:|<linkFactType>:node:|<comparison>:node:|<ringConstraint>:node:|<subTypeRule>:node:|<factTypeRule>:node:|<joinPath>:node:)<s>",
	  definition_captures
	},
	{ "factType",
	  "FactType<s>\\(<s><predicate><s>\\(<s><typename>*(<sep><typename>)\\)<s>\\)",
	  factType_captures
	},
	{ "alternatePredicate",
	  "AlternatePredicate<s>\\(<s><predicate><s>,<s><predicate>?(\\(<s>+(<roleNumber><s>)\\)<s>)\\)",
	  alternatePredicate_captures
	},
	{ "roleNaming",
	  "RoleNaming<s>\\(<s><predicateRole><sep><roleName>\\)",
	  roleNaming_captures
	},
	{ "mandatory",
	  "Mandatory<s>\\(<s><typename><sep><predicateRole>*(<sep><predicateRole>)\\)",
	  mandatory_captures
	},
	{ "unique",
	  "Unique<s>\\(<s><predicateRole>*(<sep><predicateRole>)\\)",
	  unique_captures
	},
	{ "simpleIdentification",
	  "SimpleIdentification<s>\\(<s><typename><sep><predicateRole><sep><predicateRole><s>\\)",
	  simpleIdentification_captures
	},
	{ "externalUnique",
	  "ExternalUnique<s>\\(<s><predicateRole>*(<sep><predicateRole>)\\)",
	  externalUnique_captures
	},
	{ "externalIdentification",
	  "ExternalIdentification<s>\\(<s><typename>\\(<s><predicateRole>*(<sep><predicateRole>)\\)<s>\\)",
	  externalIdentification_captures
	},
	{ "frequency",
	  "?(External:e:)Frequency<s>\\(<s><frequencyRanges><sep><predicateRole>*(<sep><predicateRole>)\\)",
	  frequency_captures
	},
	{ "frequencyRanges",
	  "\\(<s><frequencyRange>*(<sep><frequencyRange>)\\)<s>",
	  frequencyRanges_captures
	},
	{ "frequencyRange",
	  "|<naturalNumber>:low:?(\\.\\.<s>?<naturalNumber>:high:)<s>|\\.\\.<s><naturalNumber>:high:<s>",
	  frequencyRange_captures
	},
	{ "subtype",
	  "Subtype?s<s>\\(<s><typename><sep>(|<typename>|\\(<typename>*(<sep><typename>)\\))\\)",
	  subtype_captures
	},
	{ "subtypeConstrained",
	  "(|Exclusive|Exhaustive):e:Subtype?s<s>\\(<s>\\(<s><typename>*(<sep><typename>)\\)<s><typename>:super:\\)",
	  subtypeConstrained_captures
	},
	{ "subset",
	  "Subset<s>\\(<s><rolePairs>\\)",
	  subset_captures
	},
	{ "exclusive",
	  "Exclusive<s>\\(<s><rolePairs>\\)",
	  exclusive_captures
	},
	{ "equality",
	  "Equal<s>\\(<s><rolePairs>\\)",
	  equality_captures
	},
	{ "rolePairs",
	  "+<rolePair>",
	  rolePairs_captures
	},
	{ "rolePair",
	  "\\(<s><predicateRole><sep><predicateRole>\\)<s>",
	  rolePair_captures
	},
	{ "typeCardinality",
	  "TypeCardinality<s>\\(<s><typename>?,<s><cardinalityRange>\\)",
	  typeCardinality_captures
	},
	{ "roleCardinality",
	  "RoleCardinality<s>\\(<s><predicateRole>?,<s><cardinalityRange>\\)",
	  roleCardinality_captures
	},
	{ "cardinalityRange",
	  "\\(<s>(|<naturalNumber>:low:|<zero>:low:)<s>\\.\\.<s>?(|<naturalNumber>:high:|<infinity>:high:)<s>\\)<s>",
	  cardinalityRange_captures
	},
	{ "objectifies",
	  "Objectifies<s>\\(<s><typename><sep><predicate><s>\\)",
	  objectifies_captures
	},
	{ "linkFactType",
	  "LinkFactType<s>\\(<s><predicate><s>,<s><predicateRole>\\)",
	  linkFactType_captures
	},
	{ "valuesOf",
	  "ValuesOf<s>\\(<s>(|<predicateRole>:target:|<typename>:target:)\\(<s>+<range>\\)<s>\\)<s>",
	  valuesOf_captures
	},
	{ "comparison",
	  "<comparisonOperator><s>\\(<s><predicateRole><sep><predicateRole>\\)",
	  comparison_captures
	},
	{ "comparisonOperator",
	  "|<equalTo>:op:|<notEqualTo>:op:|<lessThan>:op:|<lessOrEqual>:op:|<greaterOrEqual>:op:|<greaterThan>:op:",
	  comparisonOperator_captures
	},
	{ "ringConstraint",
	  "<ringConstraintType><s>\\(<s><predicateRole><sep><predicateRole>\\)",
	  ringConstraint_captures
	},
	{ "ringConstraintType",
	  "|LocallyReflexive|PurelyReflexive|Irreflexive|Symmetric|Asymmetric|Antisymmetric|Transitive|Intransitive|StronglyIntransitive|Acyclic",
	  0
	},
	{ "subTypeRule",
	  "SubType?(Semi:semi:)Rule<s>\\(<s><typename><sep><path>\\)",
	  subTypeRule_captures
	},
	{ "factTypeRule",
	  "FactType?(Semi:semi:)Rule<s>\\(<s><predicate><sep><path>*(<sep><path>)\\)",
	  factTypeRule_captures
	},
	{ "joinPath",
	  "JoinPath<s>\\(<s><predicate><s><rolePairs>\\)",
	  joinPath_captures
	},
	{ "path",
	  "<pathDisjunction>",
	  path_captures
	},
	{ "pathDisjunction",
	  "<pathConjunction>*(<or><pathConjunction>)",
	  pathDisjunction_captures
	},
	{ "pathConjunction",
	  "<pathException>*(<and><pathException>)",
	  pathConjunction_captures
	},
	{ "pathException",
	  "<pathSimple>*(<except><pathSimple>)",
	  pathException_captures
	},
	{ "pathSimple",
	  "|<roleTraversal>:simple:|<unaryPredicate>:simple:|<typePredicate>:simple:|<valueSetPredicate>:simple:|<variableComparison>:simple:|<variableBinding>:simple:|<subexpression>:simple:",
	  pathSimple_captures
	},
	{ "roleTraversal",
	  "<predicateRole>:startRole:<arrow>+(\\[<s><predicateRole><join_operator><path>\\]<s>)",
	  roleTraversal_captures
	},
	{ "unaryPredicate",
	  "<s>&[a-z]<predicate><s>",
	  unaryPredicate_captures
	},
	{ "typePredicate",
	  "<typename><s>",
	  typePredicate_captures
	},
	{ "valueSetPredicate",
	  "\\{<s>+(<literal><s>)\\}<s>",
	  valueSetPredicate_captures
	},
	{ "variableComparison",
	  "<comparisonOperator><s><term><s>",
	  variableComparison_captures
	},
	{ "variableBinding",
	  "<variable><s>",
	  variableBinding_captures
	},
	{ "subexpression",
	  "\\(<s><path>\\)<s>",
	  subexpression_captures
	},
	{ "term",
	  "|<literal>:term:|<variable>:term:|<functionCall>:term:",
	  term_captures
	},
	{ "functionCall",
	  "<id>\\(<s>?(<term>*(<s><sep><s><term>))\\)<s>",
	  functionCall_captures
	},
	{ "variable",
	  "\\?<id>",
	  variable_captures
	},
	{ "roleName",
	  "<id><s>",
	  roleName_captures
	},
	{ "question",
	  "\\?!\\?",
	  0
	},
	{ "predicate",
	  "(|<id>:t:|<question>:t:)<s>*((|<id>:t:|<question>:t:)<s>)",
	  predicate_captures
	},
	{ "predicateRole",
	  "<predicate>\\.<roleNumber><s>",
	  predicateRole_captures
	},
	{ "roleNumber",
	  "<naturalNumber>",
	  roleNumber_captures
	},
	{ "s",
	  "*(|+[ \\t\\n\\r]|<comment_to_eol>|<comment_c_style>)",
	  0
	},
	{ "comment_to_eol",
	  "//*(!(\\n).)",
	  0
	},
	{ "comment_c_style",
	  "/\\**(!(\\*/).)\\*/",
	  0
	},
	{ "sep",
	  ",<s>",
	  0
	},
	{ "or",
	  "(|\\u2228|v!\\w)<s>",
	  0
	},
	{ "and",
	  "(|\\u2227|/\\\\)<s>",
	  0
	},
	{ "except",
	  "(|\\u2216|\\\\)<s>",
	  0
	},
	{ "join_operator",
	  "(|\\u2A1D|>\\<)<s>",
	  0
	},
	{ "literal",
	  "(|<boolean_literal>|<string>|<number>)<s>",
	  0
	},
	{ "boolean_literal",
	  "(|true!\\w<s>|false!\\w<s>)!\\w",
	  0
	},
	{ "string",
	  "\\\'*(<string_char>)\\\'",
	  0
	},
	{ "string_char",
	  "|\\\\[befntr\\\\\']|\\\\[0-7][0-7][0-7]|\\\\*[\\r][\\n]*[\\r]|\\\\0|\\\\x[0-9A-Fa-f][0-9A-Fa-f]|\\\\u[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]|(![\'\\1-\\7\\x0A-\\x1F].)",
	  0
	},
	{ "range",
	  "|<numeric_range>:range:|<string_range>:range:",
	  range_captures
	},
	{ "numeric_range",
	  "|<number>:low:<s>?(\\.\\.<s>?<number>:high:<s>)|\\.\\.<s><number>:high:<s>",
	  numeric_range_captures
	},
	{ "string_range",
	  "|<string>:low:<s>?(\\.\\.<s>?<string>:high:<s>)|\\.\\.<s><string>:high:<s>",
	  string_range_captures
	},
	{ "number",
	  "(|<real>:number:|<fractional_real>:number:|<hexnumber>:number:|<octalnumber>:number:)!\\w",
	  number_captures
	},
	{ "real",
	  "?[-+][1-9]*[0-9]?<fraction>?<exponent>",
	  0
	},
	{ "fractional_real",
	  "?[-+]0<fraction>?<exponent>",
	  0
	},
	{ "fraction",
	  "\\.+[0-9]",
	  0
	},
	{ "exponent",
	  "[Ee]?[-+]+[0-9]",
	  0
	},
	{ "naturalNumber",
	  "[1-9]*[0-9]",
	  0
	},
	{ "hexnumber",
	  "0x+[0-9A-Fa-f]",
	  0
	},
	{ "octalnumber",
	  "0*[0-7]",
	  0
	},
	{ "zero",
	  "0&(|\\.\\.|![0-9x.])",
	  0
	},
	{ "infinity",
	  "\\u221E<s>",
	  0
	},
	{ "arrow",
	  "(|=>|\\u27A4|\\u25B6)<s>",
	  0
	},
	{ "equalTo",
	  "=![>]",
	  0
	},
	{ "notEqualTo",
	  "|\\<>|\\!=|\\u2260",
	  0
	},
	{ "lessOrGreater",
	  "\\u2276",
	  0
	},
	{ "lessThan",
	  "\\<!=",
	  0
	},
	{ "lessOrEqual",
	  "|\\<=|\\u2264",
	  0
	},
	{ "greaterOrEqual",
	  "|>=|\\u2265",
	  0
	},
	{ "greaterThan",
	  ">![=<]",
	  0
	},
	{ "id",
	  "\\a*\\w",
	  0
	},
	{ "typename",
	  "<id><s>*(<id><s>)",
	  typename_captures
	}
};

template<>int	FigParser::num_rule = sizeof(FigParser::rules)/sizeof(FigParser::rules[0]);
