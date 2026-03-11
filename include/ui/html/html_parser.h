// include/ui/html/html_parser.h
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <ui/html/html_dom.h>

bool html_parse(html_doc_t* doc, const char* data, uint32_t size);
void html_free(html_doc_t* doc);