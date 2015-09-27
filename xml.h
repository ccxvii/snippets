#ifndef xml_h
#define xml_h

typedef struct xml_item xml_item;

/* UTF-8 string and return xml tree as nodes. NULL if there is a parse error. */
xml_item *xml_parse(char *buf, int preserve_white, char **error);

/* Free the XML node and all its children and siblings. */
void xml_free(xml_item *item);

/* Navigate the XML tree */
xml_item *xml_prev(xml_item *item);
xml_item *xml_next(xml_item *item);
xml_item *xml_up(xml_item *item);
xml_item *xml_down(xml_item *item);

/* xml_is_tag: Return true if the tag name matches. */
int xml_is_tag(xml_item *item, const char *name);

/* xml_tag: Return tag of XML node. Return NULL for text nodes. */
char *xml_tag(xml_item *item);

/* xml_att: Return the value of an attribute of an XML node. NULL if the attribute doesn't exist. */
char *xml_att(xml_item *item, const char *att);

/* xml_text: Return the text content of an XML node. Return NULL if the node is a tag. */
char *xml_text(xml_item *item);

xml_item *xml_find(xml_item *item, const char *tag);
xml_item *xml_find_next(xml_item *item, const char *tag);
xml_item *xml_find_down(xml_item *item, const char *tag);

#endif
