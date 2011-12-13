require "test/unit"
require "bi_gram_scoring"

class BiGramScoringTest < Test::Unit::TestCase
  def test_new
    bi_gram_scoring = BiGramScoring.new(100)
    assert_not_nil bi_gram_scoring
  end
  
  def test_add_entry_with_empty_key
    bi_gram_scoring = BiGramScoring.new(100)
    assert_equal [-1.0, nil],  bi_gram_scoring.add_entry("", "a$man$a$plan$a$canal$panama")
    assert_equal [-1.0, nil],  bi_gram_scoring.add_entry("", "")
  end
    
  def test_adding_entries_containing_no_bigrams
    bi_gram_scoring = BiGramScoring.new(100)
    assert_equal [-1.0, nil],  bi_gram_scoring.add_entry("1", "")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("2", "a")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("3", "b")
  end
    
  def test_adding_an_entry_at_capacity_should_eject_first_entry
    bi_gram_scoring = BiGramScoring.new(4)
    assert_equal [-1.0, nil],  bi_gram_scoring.add_entry("1", "a$man$a$plan$a$canal$panama")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("2", "a$man$a$plan$a$canal$panama")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("3", "a$man$a$plan$a$canal$panama")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("4", "a$man$a$plan$a$canal$panama")
    assert_equal [ 0.0, "2"],  bi_gram_scoring.add_entry("5", "a$man$a$plan$a$canal$panama")
  end

  def test_replacing_an_entry_at_capacity_should_not_eject_first_entry
    bi_gram_scoring = BiGramScoring.new(4)
    assert_equal [-1.0, nil],  bi_gram_scoring.add_entry("1", "a$man$a$plan$a$canal$panama")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("2", "a$man$a$plan$a$canal$panama")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("3", "a$man$a$plan$a$canal$panama")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("4", "a$man$a$plan$a$canal$panama")
    assert_equal [ 0.0, "1"],  bi_gram_scoring.add_entry("3", "a$man$a$plan$a$canal$panama")
  end
end
