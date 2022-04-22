#include	<stdio.h>
#if !defined(PEG_H)
#define PEG_H
/*
 * Peg parser using pegular expressions
 *
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<stdlib.h>
#include	<string.h>
#include	<pegexp.h>
#include	<vector>

template<typename TextPtr = TextPtrChar, typename Char = char, typename Capture = NullCapture>	class Peg;

template<typename TextPtr, typename Char, typename Capture>
class Peg
{
public:
	using	State = PegState<TextPtr, Char>;
	class PegexpT : public Pegexp<TextPtr, Char, Capture>
	{
		Peg<TextPtr, Char, Capture>*	peg;
	public:
		using	Pegexp = Pegexp<TextPtr, Char, Capture>;
		using	Result = typename Pegexp::Result;
		PegexpT(PegexpPC _pegexp) : Pegexp(_pegexp) {}

		void		set_closure(Peg<TextPtr, Char, Capture>* _peg) { peg = _peg; }
		virtual Result	match_extended(State& state)
		{
			if (*state.pc == '<')
				return peg->recurse(state);
			else
				return Pegexp::match_literal(state);
		}

		// Null extension; treat extensions like literal characters
		virtual void	skip_extended(PegexpPC& pc)
		{
			if (*pc++ == '<')
				while (*pc != '\0' && *pc++ != '>')
					;
		}
	};
	using	Result = typename PegexpT::Result;

	typedef struct {
		const char*	name;
		PegexpT		expression;
	} Rule;

	/*
	 * The rules must be sorted by name, such that a binary search works
	 */
	Peg(Rule* _rules, int _num_rule)
			: rules(_rules), num_rule(_num_rule)
			{
				qsort(rules, num_rule, sizeof(rules[0]), [](const void* r1, const void* r2) {
					return strcmp(((const Rule*)r1)->name, ((const Rule*)r2)->name);
				});
			}

	Result	parse(TextPtr text)
			{
				Rule*	top = lookup("TOP");
				assert(top);

#if defined(PEG_TRACE)
				printf("Calling %s at `%.10s...` (pegexp `%s`)\n", top->name, (const char*)text, top->expression.code());
#endif

				nesting.push_back({top, text});
				top->expression.set_closure(this);
				typename PegexpT::Result result = top->expression.match_here(text);
				nesting.pop_back();
				return result;
			}

	Rule*	lookup(const char* name)
			{
				Rule*	rule = (Rule*)bsearch(
					name, rules, num_rule, sizeof(rules[0]),
					[](const void* key, const void* r2) {
						const char*	key_name = (const char*)key;
						const char*	rule_name = ((const Rule*)r2)->name;
						int		len = strlen(rule_name);

						int		cval = strncmp(key_name, rule_name, len);
						if (cval == 0 && (key_name[len] != '>' && key_name[len] != '\0'))
							cval = 1;	// This rule has a shortened version of this key
						return cval;
					}
				);
				if (!rule)
					return 0;
				return rule;
			}

	Result		recurse(State& state)
			{
				State	start_state(state);
				state.pc++;	// Skip the '<'

				// Find the end of the rule name.
				const char*	brangle = strchr(state.pc, '>');
				if (!brangle)
					brangle = state.pc+strlen(state.pc);	// Error, no > before end of expression

				Rule*	sub_rule = lookup(state.pc);
				if (!sub_rule)
				{
#if defined(PEG_TRACE)
					printf("failed to find rule `%.*s`\n", (int)(brangle-state.pc), state.pc);
					// REVISIT: Call a PEG callout to enable custom-coded rules?
#endif
					return Result::fail(start_state);
				}

				// Check for left recursion:
				for (int i = 1; i <= nesting.size(); i++)
				{
					Nesting&	frame = nesting[nesting.size()-i];
					if (frame.rule == sub_rule
					 && frame.text == state.text)
					{
#if defined(PEG_TRACE)
						printf("Left recursion detected on %s at `%.10s`\n", frame.rule->name, (const char*)state.text);
						for (int j = 0; j < nesting.size(); j++)
							printf("%s%s", nesting[j].rule->name, j < nesting.size() ? "->" : "\n");
#endif
						return Result::fail(start_state);
					}
				}
				nesting.push_back({sub_rule, state.text});

				State		substate = state;
				substate.pc = sub_rule->expression.code();
#if defined(PEG_TRACE)
				printf("Calling %s at `%.10s...` (pegexp `%s`)\n", sub_rule->name, (const char*)state.text, sub_rule->expression.code());
#endif
				sub_rule->expression.set_closure(this);
				Result	result = sub_rule->expression.match_here(substate);

#if defined(PEG_TRACE)
				for (int j = 0; j < nesting.size(); j++)
					printf("%s%s", nesting[j].rule->name, j < nesting.size() ? "->" : " ");
#endif
				if (result)
				{
					TextPtr	from = state.text;

					state.pc = brangle;
					if (*state.pc == '>')	// Could be NUL on ill-formed input
						state.pc++;
					state.text = substate.text;
#if defined(PEG_TRACE)
					printf("MATCH `%.*s`\n", (int)(state.text-from), (const char*)from);
					printf("continuing at text `%.10s`...\n", (const char*)state.text);
#endif
				}
#if defined(PEG_TRACE)
				else
					printf("FAIL\n");
#endif
				nesting.pop_back();
				return result;
			}

protected:
	Rule*		rules;
	int		num_rule;
	typedef struct {
		Rule*	rule;
		TextPtr	text;
	} Nesting;
	std::vector<Nesting>	nesting;
};
#endif	// PEG_H
