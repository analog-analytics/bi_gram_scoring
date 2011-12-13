Gem::Specification.new do |s|
  s.name        = 'bi_gram_scoring'
  s.version     = '0.0.1'
  s.date        = '2011-12-13'
  s.summary     = "String similarity scoring"
  s.description = "String similarity scoring using bi-gram frequency"
  s.authors     = ["Tom Buscher"]
  s.email       = "support@analoganalytics.com"
  s.homepage    = "http://github.com/analog-analytics/bi_gram_scoring"
  s.files       = Dir.glob('lib/**/*.rb') + Dir.glob('ext/**/*.{c,h,rb}')
  s.extensions  = ['ext/bi_gram_scoring/extconf.rb']
end