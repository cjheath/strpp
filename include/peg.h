#include	<cstdio>
#if !defined(PEG_H)
#define PEG_H
/*
 * Peg parser using pegular expressions
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<cstdlib>
#include	<cstring>
#include	<pegexp.h>
#include	<vector>

template<
	typename TextPtr = PegexpDefaultInput,
	typename Capture = NullCapture<PegexpDefaultInput>
>
class Peg
{
public:
	using	PChar = typename TextPtr::Char;
	using	State = PegState<TextPtr>;
	class PegexpT : public Pegexp<TextPtr, Capture>
	{
		Peg<TextPtr, Capture>*	peg;
	public:
		using	Pegexp = Pegexp<TextPtr, Capture>;
		PegexpT(PegexpPC _pegexp) : Pegexp(_pegexp) {}

		void		set_closure(Peg<TextPtr, Capture>* _peg) { peg = _peg; }
		virtual State	match_extended(State& state, Capture& capture)
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

	State	parse(TextPtr text)
			{
				Rule*	top = lookup("TOP");
				assert(top);

#if defined(PEG_TRACE)
				printf("Calling %s at `%.10s...` (pegexp `%s`)\n", top->name, text.peek(), top->expression.code());
#endif

				nesting.push_back({top, text});
				top->expression.set_closure(this);
				Capture		capture;	// Commence a new Capture
				State	result = top->expression.match_here(text, capture);
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

	State		recurse(State& state)
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
#endif
					return start_state.fail();
				}

				// Check for left recursion:
				for (int i = 1; i <= nesting.size(); i++)
				{
					Nesting&	frame = nesting[nesting.size()-i];
					if (frame.rule == sub_rule
					 && frame.text == state.text)
					{
#if defined(PEG_TRACE)
						printf("Left recursion detected on %s at `%.10s`\n", frame.rule->name, state.text.peek());
						for (int j = 0; j < nesting.size(); j++)
							printf("%s%s", nesting[j].rule->name, j < nesting.size() ? "->" : "\n");
#endif
						return start_state.fail();
					}
				}
				nesting.push_back({sub_rule, state.text});

				State		substate = state;
				substate.pc = sub_rule->expression.code();

#if defined(PEG_TRACE)
				printf("Calling %s at `%.10s...` (pegexp `%s`)\n", sub_rule->name, state.text.peek(), sub_rule->expression.code());
#endif
				sub_rule->expression.set_closure(this);
				Capture		capture;	// Commence a new Capture
				State	result = sub_rule->expression.match_here(substate, capture);

#if defined(PEG_TRACE)
				for (int j = 0; j < nesting.size(); j++)
					printf("%s%s", nesting[j].rule->name, j < nesting.size() ? "->" : " ");
#endif
				if (result)
				{
					TextPtr		from = state.text;

					state.pc = brangle;
					if (*state.pc == '>')	// Could be NUL on ill-formed input
						state.pc++;
					state.text = substate.text;

#if 0	/* REVISIT: Only include if we want to capture the results of recursive rule calls */
					if (*state.pc != ':')	// Only save if subrule is not labelled
						capture.save(sub_rule->name, from, state.text);
#endif

#if defined(PEG_TRACE)
					printf("MATCH `%.*s`\n", (int)state.text.bytes_from(from), from.peek());
					printf("continuing at text `%.10s`...\n", state.text.peek());
#endif
				}
#if defined(PEG_TRACE)
				else
					printf("FAIL\n");
#endif
				nesting.pop_back();
				if (!result)
					state = start_state.fail();
				return state;
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
