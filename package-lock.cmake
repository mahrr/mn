# CPM Package Lock
# This file should be committed to version control

# fmt
CPMDeclarePackage(fmt
  NAME fmt
  GIT_TAG 9.1.0
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_SHALLOW TRUE
)
# doctest
CPMDeclarePackage(doctest
  NAME doctest
  GIT_TAG 2.4.6
  GIT_REPOSITORY https://github.com/onqtam/doctest.git
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL TRUE
)
# nanobench
CPMDeclarePackage(nanobench
  NAME nanobench
  GIT_TAG v4.3.5
  GIT_REPOSITORY https://github.com/martinus/nanobench.git
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL TRUE
)
