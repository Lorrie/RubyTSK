require 'bundler'
require 'rspec/core/rake_task'
require 'rake/extensiontask'
Rake::ExtensionTask.new do |ext|
	ext.name = 'tsk4r' # name for Ruby C part of sleuthkit module
	ext.ext_dir = 'ext/tsk4r'
	ext.lib_dir = 'lib/tsk4r'
end
Bundler::GemHelper.install_tasks

RSpec::Core::RakeTask.new('spec')

SPEC_SUITES = [
{ :id => :image, :title => 'Disk Images', :pattern => 'spec/image/image_spec.rb' },
{ :id => :volume, :title => 'Volumes', :pattern => 'spec/volume/*_spec.rb' },
{ :id => :filesystem, :title => 'File Systems', :pattern => 'spec/filesystem/filesystem_spec.rb' },
{ :id => :experiments, :title => 'Experiments', :pattern => 'spec/exp/*_spec.rb' }
]
namespace :spec do
  namespace :suite do
    SPEC_SUITES.each do |suite|
      desc "Run all specs in #{suite[:title]} spec suite"
      RSpec::Core::RakeTask.new(suite[:id]) do |t|
        # to do: add opts for given suite, if present
        t.pattern = suite[:pattern]
      end
    end
  end
end

# helper to run clean, build and compile
desc "Cleans, Builds and Compiles"
task :cbc => [ :clean, :build, :compile ] do
	  puts "Cleaned, Built, Compiled."
end

# add other tasks found in lib/tasks/*.rake
Dir.glob('tasks/*.rake').each { |r| import r }
