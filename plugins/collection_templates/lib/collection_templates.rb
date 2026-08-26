# =========================================================================
#   collection_templates - project-local Ceedling plugin
# =========================================================================
#
# Ceedling's module generator can only *prepend* text to a generated file
# (:module_generator: :boilerplates: in project.yml). Everything below that -
# the header's include guard, the source file's #include of it - comes from
# Unity's own templates, so a boilerplate cannot put anything *after* the
# #include block.
#
# Unity declares those templates with `||=` (vendor/unity/auto/generate_module.rb),
# which means a template constant that already exists when that file is required
# wins. `ceedling module:create` requires the generator lazily, at task time,
# while plugins are loaded during configuration - so defining TEMPLATE_INC and
# TEMPLATE_SRC here is enough to replace the stock templates.
#
# The templates themselves live in assets/. Their format specifiers are the ones
# Unity passes to String#% :
#
#   %1$s  module name as spelled on disk (snake case here) -> arraylist
#   %2$s  the matching :includes: block (:src: or :inc:), one #include per
#         line, with a trailing newline
#   %3$s  module name upcased, '-' turned into '_'         -> ARRAYLIST
#
# A literal % in the template must be written %% or String#% will choke.

require 'ceedling/plugins/plugin'

class CollectionTemplates < Plugin
  ASSETS = File.join( __dir__, '..', 'assets' )

  # Unity looks its template constants up at top level, so that is where they
  # have to be defined. Removing one first keeps Ruby quiet in the unlikely
  # case something required the generator before this plugin was loaded.
  def self.define_template(constant, filename)
    Object.send( :remove_const, constant ) if Object.const_defined?( constant )
    Object.const_set( constant, File.read( File.join( ASSETS, filename ) ).freeze )
  end
end

CollectionTemplates.define_template( :TEMPLATE_INC, 'header_template.h' )
CollectionTemplates.define_template( :TEMPLATE_SRC, 'source_template.c' )
