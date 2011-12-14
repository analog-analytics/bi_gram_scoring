# bi_gram_scoring: string similarity scoring using bi-grams

bi_gram_scoring maintains a list of strings, and as each new string is added,
provides a similarity score for the added string compared against the strings
already in the list. Similarity scores are calculated using the frequency of
all two-character combinations (bi-grams) in two strings being scored.

## Usage

First, create a bi_gram_scoring instance:

    require 'bi_gram_scoring'
    bi_gram_scoring = BiGramScoring.new(500)

The parameter passed to `BiGramScoring#new` is the _capacity_ of the instance.
Once the instance reaches its capacity, adding a new entry ejects the first
entry, so in effect the instance retains a rolling window of the added entries.

To add a new entry:

    score, matching_key, matching_arg = bi_gram_scoring.add_entry(key, value, arg)

where:

 * `key` is a string used to identify the entry. If an entry is added with
   the same key as a retained entry, the existing entry is overwritten;

 * `value` is the string that will be scored;

 * `arg` is a pass-through value associated with this entry.

The method returns the maximum `score` (a value between 0.0 and 1.0) of the new
entry, compared against all other retained entries, as well as the key and arg
values of the retained entry having the maximum score against the new entry.



