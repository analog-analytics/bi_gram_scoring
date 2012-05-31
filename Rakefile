require 'rake/testtask'
require 'rake/clean'

NAME = 'bi_gram_scoring'

DLEXT = RbConfig::CONFIG['DLEXT']

file "lib/#{NAME}/#{NAME}.#{DLEXT}" => Dir.glob("ext/#{NAME}/*{.rb,.c}") do
  Dir.chdir("ext/#{NAME}") do
    ruby "extconf.rb"
    sh "make"
  end
  cp "ext/#{NAME}/#{NAME}.#{DLEXT}", "lib/#{NAME}"
end

task :test => "lib/#{NAME}/#{NAME}.#{DLEXT}"

CLEAN.include('ext/**/*{.o,.log,.so,.bundle}')
CLEAN.include('ext/**/Makefile')
CLOBBER.include('lib/**/*{.so,.bundle}')

Rake::TestTask.new do |t|
  t.libs << 'test'
end

desc "Run tests"
task :default => :test
