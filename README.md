# The Quatalog

Use the live site here: https://quatalog.com

The official RPI course catalog is well-known for being a garbage fire.
Just about every piece of information that it gives for a course can be inaccurate.

Oftentimes, the prerequisites are wrong, the times that the course is offered is wrong ("Spring annually"/etc),
and worst of all many courses still listed in the catalog haven't been offered in years and effectively no longer exist (e.g. [CHEM-4330](https://catalog.rpi.edu/preview_course.php?catoid=24&coid=51428), which is still listed as "Fall anually," hasn't been offered since 2013: see [CHEM-4330's Quatalog page](https://quatalog.com/courses/CHEM-4330)).

The Quatalog aims to make course planning easier by providing accurate prerequiste data and data for when a
given course was offered in the past, as well as data for how many students were enrolled in the course,
and the professor(s) teaching during a given semester.

The name comes from [QuACS](https://github.com/quacs/quacs), which is the [source of data](https://github.com/quacs/quacs-data) for this project. I have also contributed to QuACS a bit (primarily their SIS scraper; I was not interested in writing my own which is why we use their data) while working on the Quatalog.

This is the main quatalog repo, which includes the data scraper (converts QuACS data into the Quatalog's data format). To see the data for this project, see the [data repo](https://github.com/quatalog/data), and to see the static site, see the [site repo](https://github.com/quatalog/site).

The Quatalog project is a memeber of the [Rensselaer Center for Open Source (RCOS)](https://rcos.io).
