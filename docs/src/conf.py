import os
import re

conf_dir = os.path.dirname(os.path.abspath(__file__))

# -- Project information -----------------------------------------------------

project = 'MIEM'
copyright = '2026, University Corporation for Atmospheric Research'
author = 'NCAR'

regex = r'project\(.*VERSION\s+(\d+\.\d+\.\d+)'
version = '0.0.0'
cmake_file = os.path.join(conf_dir, '..', '..', 'CMakeLists.txt')
with open(cmake_file, 'r') as f:
    content = f.read()
    match = re.search(regex, content)
    if match:
        version = match.group(1)
release = version

# -- General configuration ---------------------------------------------------

extensions = [
    'breathe',
    'sphinx.ext.mathjax',
    'sphinx_copybutton',
    'sphinx_design',
]

# -- Breathe configuration ---------------------------------------------------

this_dir = os.path.dirname(os.path.abspath(__file__))
breathe_projects_dir = os.path.abspath(
    os.path.join(this_dir, '..', '..', 'build', 'docs', 'doxygen', 'xml')
)
breathe_projects = {'miem': breathe_projects_dir}
breathe_default_project = 'miem'

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_book_theme'
html_theme_options = {
    'repository_url': 'https://github.com/NCAR/miem',
    'use_repository_button': True,
    'use_issues_button': True,
    'use_edit_page_button': False,
}

# -- General ---------------------------------------------------

exclude_patterns = ['_build']
templates_path = []
