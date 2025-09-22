/*
 * Rules for a PxParser
 *
 * You must declare this type in px_parser.h by expanding the Peg<> template
 */
#include	<px_parser.h>

const char*	TOP_captures[] = { "rule", 0 };
const char*	rule_captures[] = { "name", "alternates", "action", 0 };
const char*	action_captures[] = { "function", "parameter", 0 };
const char*	parameter_captures[] = { "parameter", 0 };
const char*	reference_captures[] = { "name", "joiner", 0 };
const char*	alternates_captures[] = { "sequence", 0 };
const char*	sequence_captures[] = { "repetition", 0 };
const char*	repeat_count_captures[] = { "limit", 0 };
const char*	count_captures[] = { "val", 0 };
const char*	repetition_captures[] = { "repeat_count", "atom", "label", 0 };
const char*	label_captures[] = { "name", 0 };
const char*	atom_captures[] = { "any", "call", "property", "literal", "class", "group", 0 };
const char*	group_captures[] = { "alternates", 0 };

template<>PxParser::Rule	PxParser::rules[] =
{
	{ "EOF",
	  "!.",
	  0
	},
	{ "space",
	  "|[ \\t\\r\\n]|//*[^\\n]",
	  0
	},
	{ "blankline",
	  "\\n*[ \\t\\r](|\\n|<EOF>)",
	  0
	},
	{ "s",
	  "*(!<blankline><space>)",
	  0
	},
	{ "TOP",
	  "*<space><rule>",
	  TOP_captures
	},
	{ "rule",
	  "<name><s>=<s><alternates>?<action><blankline>*<space>",
	  rule_captures
	},
	{ "action",
	  "-><s>?(<name>:function:\\:<s>)<parameter>*(,<s><parameter>)<s>",
	  action_captures
	},
	{ "parameter",
	  "(|<reference>:parameter:|\\\'<literal>:parameter:\\\')<s>",
	  parameter_captures
	},
	{ "reference",
	  "<name><s>*([.*]:joiner:<s><name>)",
	  reference_captures
	},
	{ "alternates",
	  "|+(\\|<s><sequence>)|<sequence>",
	  alternates_captures
	},
	{ "sequence",
	  "*<repetition>",
	  sequence_captures
	},
	{ "repeat_count",
	  "|[?*+!&]:limit:<s>|<count>:limit:",
	  repeat_count_captures
	},
	{ "count",
	  "\\{(|(+\\d):val:|<name>:val:)<s>\\}<s>",
	  count_captures
	},
	{ "repetition",
	  "?<repeat_count><atom>?<label><s>",
	  repetition_captures
	},
	{ "label",
	  "\\:<name>",
	  label_captures
	},
	{ "atom",
	  "|\\.:any:|<name>:call:|\\\\<property>|\\\'<literal>\\\'|\\[<class>\\]|\\(<group>\\)",
	  atom_captures
	},
	{ "group",
	  "<s>+<alternates>",
	  group_captures
	},
	{ "name",
	  "[\\a_]*[\\w_]",
	  0
	},
	{ "literal",
	  "*(![\']<literal_char>)",
	  0
	},
	{ "literal_char",
	  "|\\\\(|?[0-3][0-7]?[0-7]|x\\h?\\h|x\\{+\\h\\}|u\\h?\\h?\\h?\\h|u\\{+\\h\\}|[^\\n])|[^\\\\\\n]",
	  0
	},
	{ "property",
	  "[adhswLU]",
	  0
	},
	{ "class",
	  "?\\^?-+<class_part>",
	  0
	},
	{ "class_part",
	  "!\\]<class_char>?(-!\\]<class_char>)",
	  0
	},
	{ "class_char",
	  "![-\\]]<literal_char>",
	  0
	}
};

template<>int	PxParser::num_rule = sizeof(PxParser::rules)/sizeof(PxParser::rules[0]);
