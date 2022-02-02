#include "parser.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


bool type_equals(Type *a, Type *b)
{
    if (a == NULL && b == NULL)
        return true;
    if (a == NULL || b == NULL)
        return false;
    return a->type == b->type && type_equals(a->ptr, b->ptr);
}

i64 size_for_type(Type *type)
{
    switch (type->type)
    {
    case TYPE_INT: return 8;
    case TYPE_PTR: return 8;
    default: assert(false && "Unreachable type");
    }
}

Type *type_new(DataType type)
{
    // For the core types, we don't need to allocate any memory, just
    // return a pointer to a static instance.
    static Type type_int = {.type = TYPE_INT, .ptr = NULL};
    if (type == TYPE_INT) return &type_int;
    
    Type *self = calloc(sizeof(Type), 1);
    self->type = type;
    return self;
}

static char *data_type_to_str(DataType type)
{
    switch (type)
    {
    case TYPE_NONE: return "void";
    case TYPE_INT: return "int";
    case TYPE_PTR: return "*";
    default: assert(false && "Unreachable");
    }
}

char *type_to_str(Type *type)
{
    // FIXME: Handle array types.
    
    // TODO: This allocates memory and we probably don't want to do that.
    // TODO: Probably want to increase this size once we have longer types
    char *str = calloc(sizeof(char), 32);
    int ptr_count = 0;
    for (; type->type == TYPE_PTR; type = type->ptr)
        ptr_count++;
    
    
    // FIXME: This is inefficient as all hell but this will only really be
    //        used for error reporting.
    strcat(str, data_type_to_str(type->type));
    for (int i = 0; i < ptr_count; i++)
        strcat(str, "*");
    return str;
}

Node *handle_unary_expr_types(Node *node, Token *token)
{
    Type *old_type = node->unary_expr->expr_type;
    
    if (node->type == OP_NOT) {
        node->expr_type = type_new(TYPE_INT);
    } else if (node->type == OP_ADDROF) {
        Type *ptr = type_new(TYPE_PTR);
        ptr->ptr = old_type;
        node->expr_type = ptr;
    } else if (node->type == OP_DEREF) {
        if (old_type->type != TYPE_PTR)
            die_location(token->loc, "Cannot dereference non-pointer type");
        node->expr_type = old_type->ptr;
    } else if (node->type == OP_NEG) {
        if (old_type->type != TYPE_INT)
            die_location(token->loc, "Cannot negate non-integer type");
        node->expr_type = type_new(TYPE_INT);
    } else {
        // Default to not changing the type
        node->expr_type = old_type;
    }
    // die_location(token->loc, "Unknown unary expression type in handle_unary_expr_types\n");
    return node;
}

Node *handle_binary_expr_types(Node *node, Token *token)
{
    Type *left = node->binary.left->expr_type;
    Type *right = node->binary.right->expr_type;
    
    switch (node->type) 
    {
        case OP_PLUS: {
            if (left->type == TYPE_INT && right->type == TYPE_INT) {
                node->expr_type = type_new(TYPE_INT);
            } else if (left->type == TYPE_PTR && right->type == TYPE_INT) {
                node->expr_type = left;
                // Pointer arithmetic!
                Node *mul = Node_new(OP_MUL);
                mul->binary.left = node->binary.right;
                mul->binary.right = Node_new(AST_LITERAL);
                mul->binary.right->literal.type = type_new(TYPE_INT);
                mul->binary.right->literal.as_int = size_for_type(left->ptr);
                node->binary.right = mul;
            } else if (left->type == TYPE_INT && right->type == TYPE_PTR) {
                node->expr_type = right;
                // Pointer arithmetic!
                Node *mul = Node_new(OP_MUL);
                mul->binary.left = node->binary.left;
                mul->binary.right = Node_new(AST_LITERAL);
                mul->binary.right->literal.type = type_new(TYPE_INT);
                mul->binary.right->literal.as_int = size_for_type(right->ptr);
                node->binary.left = mul;
            } else {
                die_location(token->loc, "Cannot add non-integer types");
            }
        } break;

        case OP_MINUS: {
            if (left->type == TYPE_INT && right->type == TYPE_INT) {
                node->expr_type = type_new(TYPE_INT);
            } else if (left->type == TYPE_PTR && right->type == TYPE_INT) {
                node->expr_type = left;
                // Pointer arithmetic!
                Node *mul = Node_new(OP_MUL);
                mul->binary.left = node->binary.right;
                mul->binary.right = Node_from_int_literal(size_for_type(left->ptr));
                node->binary.right = mul;
            } else if (left->type == TYPE_INT && right->type == TYPE_PTR) {
                node->expr_type = right;
                // Pointer arithmetic!
                Node *mul = Node_new(OP_MUL);
                mul->binary.left = node->binary.left;
                mul->binary.right = Node_from_int_literal(size_for_type(right->ptr));
                node->binary.left = mul;
            } else if (left->type == TYPE_PTR && right->type == TYPE_PTR) {
                // TODO: Check for different pointer types
                node->expr_type = type_new(TYPE_INT);
                // Divide by size of pointer
                Node *div = Node_new(OP_DIV);
                div->binary.left = node;
                div->binary.right = Node_from_int_literal(size_for_type(left->ptr));
                div->expr_type = node->expr_type;
                node = div;
            } else {
                die_location(token->loc, "Cannot subtract non-integer types");
            }
        } break;

        case OP_DIV:
        case OP_MOD:
        case OP_MUL: {
            if (left->type == TYPE_INT && right->type == TYPE_INT) {
                node->expr_type = left;
            } else {
                die_location(token->loc, "Cannot do operation `%s` non-integer types", node_type_to_str(node->type));
            }
        } break;

        case OP_EQ:
        case OP_NEQ:
        case OP_LT:
        case OP_GT:
        case OP_LEQ:
        case OP_GEQ:
        case OP_AND:
        case OP_OR: {
            node->expr_type = type_new(TYPE_INT);
        } break;

        default:
            die_location(token->loc, "Unknown binary expression type in handle_binary_expr_types\n");
    }
    return node;
}
