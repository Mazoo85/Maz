"""Optional per-site Python extractors for messy pages.

Drop a module here named to match a recipe's ``hook:`` value. It must define::

    def extract(html: str, url: str) -> list[dict]:
        ...

Return a list of record dicts. When a recipe names a hook, it runs *instead* of
the recipe's declarative ``fields`` selectors, so a hook has full control over
how a page becomes records.
"""
