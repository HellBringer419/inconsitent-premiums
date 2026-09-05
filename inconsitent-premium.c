#include <cjson/cJSON.h>
#include <fyers_api.h>
#include <fyers_model.h>
#include <fyers_session.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

fyers_model_t *get_fyers_model(fyers_session_t *session);
void process_market_data(fyers_model_t *model);
double get_future_ltp(fyers_model_t *model, const char *expiry);
int print_option_chain_output(cJSON *data, double nifty_fut_last_price);

int main(void) {
  fyers_session_t *session = fyers_session_create(
      "STRONGPASSWORD-200", "http://localhost:port/fyers/redirect", "SEC123T");
  fyers_model_t *model = get_fyers_model(session);
  if (model == NULL) {
    return -1;
  }

  // puts("Fyers is now ready to use");

  process_market_data(model);

  fyers_model_destroy(model);
  fyers_session_destroy(session);
  fyers_cleanup();

  return 0;
}

fyers_model_t *get_fyers_model(fyers_session_t *session) {
  char buffer[1024];

  fyers_model_t *model;
  FILE *file_read_ptr;

  if ((file_read_ptr = fopen("./data/access_token.txt", "r")) == NULL) {
    generate_authcode(session);

    puts("Enter auth code:");
    scanf("%1022s", buffer);
    fyers_session_set_authcode(session, buffer);
    fyers_response_t *token_response = generate_token(session);
    printf("resp: data-> %s \n", token_response->data);
    cJSON *token = cJSON_Parse(token_response->data);
    if (token == NULL) {
      const char *error_ptr = cJSON_GetErrorPtr();
      if (error_ptr != NULL) {
        fprintf(stderr, "Error: No Token %s\n", error_ptr);
        cJSON_Delete(token);
        fyers_response_destroy(token_response);
        return NULL;
      }
    }
    const char *access_token_string = NULL;
    const cJSON *acess_token_cjson = cJSON_GetObjectItemCaseSensitive(token, "access_token");
    if (!cJSON_IsString(acess_token_cjson)) {
      fprintf(stderr, "token response is not a string");
      cJSON_Delete(token);
        fyers_response_destroy(token_response);
        return NULL;
    } else {
      access_token_string = acess_token_cjson->valuestring;
    }

    const char *client_id = fyers_session_get_client_id(session);
    model = fyers_model_create(client_id, access_token_string, false, NULL,
                               FYERS_LOG_INFO);

    if (!model) {
      fprintf(stderr, "Failed to create model");
      return NULL;
    }

    // save for later use
    FILE *file_write_ptr;
    if ((file_write_ptr = fopen("./data/access_token.txt", "w")) == NULL) {
      fprintf(stderr,
              "File or directory (data/access_token.txt) couldnt be created");
      return NULL;
    }
    printf("%zd 3", strlen(access_token_string));
    // FIXME: Not working correctly
    fprintf(file_write_ptr, "%s", access_token_string);

    fclose(file_write_ptr);
  } else {
    memset(buffer, 0, strlen(buffer));
    fscanf(file_read_ptr, "%1023s", buffer);
    const char *client_id = fyers_session_get_client_id(session);
    const char *access_token = buffer;
    model = fyers_model_create(client_id, access_token, false, NULL,
                               FYERS_LOG_INFO);
    fclose(file_read_ptr);
  }

  return model;
}

double get_future_ltp(fyers_model_t *model, const char *expiry) {
  double nifty_fut_last_price = 0;
  fyers_response_t *response_fut =
      fyers_model_get_quotes(model, "NSE:NIFTY26SEPFUT");
  cJSON *nifty_fut = cJSON_Parse(response_fut->data);
  if (nifty_fut == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error before: %s\n", error_ptr);
      cJSON_Delete(nifty_fut);
      fyers_response_destroy(response_fut);
      return -1;
    }
  }

  const cJSON *depth = NULL;
  const cJSON *depths = cJSON_GetObjectItemCaseSensitive(nifty_fut, "d");
  cJSON_ArrayForEach(depth, depths) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(depth, "n");
    const char *name = NULL;

    if (!cJSON_IsString(n)) {
      fprintf(stderr, "Error: %s",
              "response_fut->data[d][0][n] is not a string\n");
      cJSON_Delete(nifty_fut);
      fyers_response_destroy(response_fut);
      return -1;
    } else {
      name = n->valuestring;
    }

    const cJSON *v = cJSON_GetObjectItemCaseSensitive(depth, "v");
    const cJSON *lp = cJSON_GetObjectItemCaseSensitive(v, "lp");

    if (!cJSON_IsNumber(lp)) {
      fprintf(stderr, "Erro: %s",
              "response_fut->data[d][0][lp] is not a number");
      cJSON_Delete(nifty_fut);
      fyers_response_destroy(response_fut);
      return -1;
    } else {
      nifty_fut_last_price = lp->valuedouble;
      printf("%s last price: %.2f\n", name, nifty_fut_last_price);
    }
  }

  cJSON_Delete(nifty_fut);
  fyers_response_destroy(response_fut);
  return nifty_fut_last_price;
}

int print_option_chain_output(cJSON *data, double nifty_fut_last_price) {
  const cJSON *chain_item = NULL;
  cJSON *options_chain = cJSON_GetObjectItemCaseSensitive(data, "optionsChain");

  double nifty_ltp = 0;
  int atm = 0;
  // to remember last stike
  int last_strike = 0;
  const char *last_ce_pe = "PE";
  double last_ltp = 0;

  cJSON_ArrayForEach(chain_item, options_chain) {
    const cJSON *ltp = cJSON_GetObjectItemCaseSensitive(chain_item, "ltp");
    const cJSON *strike_price =
        cJSON_GetObjectItemCaseSensitive(chain_item, "strike_price");
    const cJSON *option_type =
        cJSON_GetObjectItemCaseSensitive(chain_item, "option_type");
    const cJSON *symbol =
        cJSON_GetObjectItemCaseSensitive(chain_item, "symbol");

    if (!cJSON_IsNumber(ltp) || !cJSON_IsNumber(strike_price) ||
        !cJSON_IsString(symbol) || !cJSON_IsString(option_type)) {
      fprintf(
          stderr, "Error: %s or %s",
          "option_chain[ltp] || option_chain[strike_price] is not a number ",
          "option_chain[option_type] || option_chain[symbol] is not a "
          "string\n");
      return -1;
    } else {
      if (strike_price->valueint == -1) {
        // this is the NIFTY entry
        nifty_ltp = ltp->valuedouble;
        const char *nifty_symbol = symbol->valuestring;
        atm = round(nifty_ltp / 100) * 100;

        printf("%s nifty ltp: %.2f and atm %d\n", nifty_symbol, nifty_ltp, atm);
        printf("ltp, strike, CE/PE \t strike + fut price (diff) \t strike + "
               "nifty ltp (diff) \t last strike diff (ltp diff)\n");
      } else {
        double ltp_item = ltp->valuedouble;
        int strike = strike_price->valueint;
        const char *ce_pe = option_type->valuestring;
        const char *symbol_item = symbol->valuestring;

        const int strike_diff = strike - last_strike;
        const double ltp_diff = ltp_item - last_ltp;
        const int ce_pe_diff = strncmp(ce_pe, last_ce_pe, 2);
        // printf("DEBUG: %d - %d = (%d), %f - %f = (%f), %s (%s) \n", strike,
        //        last_strike, strike_diff, ltp_item, last_ltp, ltp_diff, ce_pe,
        //        last_ce_pe);

        if (strike < atm && strncmp(ce_pe, "CE", 2) == 0) {
          const double diff_strike_with_fut = nifty_fut_last_price - strike;
          const int diff_strike_with_nifty = nifty_ltp - strike;

          printf("%.2f %d %s: \t ", ltp_item, strike, ce_pe);
          printf("%.2f (%.2f) \t\t ", diff_strike_with_fut,
                 ltp_item - diff_strike_with_fut);
          printf("%d (%.2f) \t\t ", diff_strike_with_nifty,
                 ltp_item - diff_strike_with_nifty);
          printf("%d (%.2f) \n", ce_pe_diff == 0 ? strike_diff : 0,
                 ce_pe_diff == 0 ? ltp_diff : 0);

          last_ltp = ltp_item;
          last_strike = strike;
          last_ce_pe = ce_pe;
        } else if (strike > atm && strncmp(ce_pe, "PE", 2) == 0) {
          double diff_strike_with_fut = strike - nifty_fut_last_price;
          int diff_strike_with_nifty = strike - nifty_ltp;

          printf("%.2f %d %s: \t ", ltp_item, strike, ce_pe);
          printf("%.2f (%.2f) \t\t ", diff_strike_with_fut,
                 ltp_item - diff_strike_with_fut);
          printf("%d (%.2f) \t ", diff_strike_with_nifty,
                 ltp_item - diff_strike_with_nifty);
          printf("%d (%.2f) \n", ce_pe_diff == 0 ? strike_diff : 0,
                 ce_pe_diff == 0 ? ltp_diff : 0);

          last_ltp = ltp_item;
          last_strike = strike;
          last_ce_pe = ce_pe;
        } else if (strike == atm && strncmp(ce_pe, "CE", 2) == 0) {
          puts("-------- \tATM\t -----------\n");
        }
      }
    }
  }

  return 0;
}

void process_market_data(fyers_model_t *model) {
  double nifty_fut_last_price = get_future_ltp(model, "?");
  if (nifty_fut_last_price == -1) {
    fprintf(stderr, "Error: %s\n", "Sent earlier");
  }

  cJSON *query_json = cJSON_CreateObject();
  const char *symbol = "NSE:NIFTY50-INDEX";
  cJSON_AddStringToObject(query_json, "symbol", symbol);
  cJSON_AddNumberToObject(query_json, "strikecount", 20);
  cJSON_AddStringToObject(query_json, "greeks", "0");
  cJSON_AddStringToObject(query_json, "timestamp", "1790676600");

  const char *params = cJSON_Print(query_json);

  fyers_response_t *response_quotes =
      fyers_model_get_option_chain(model, params);
  cJSON *response_json = cJSON_Parse(response_quotes->data);
  if (response_json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error: %s\n", error_ptr);
    }
  }
  // access the JSON data
  cJSON *data = cJSON_GetObjectItemCaseSensitive(response_json, "data");
  const int chain = print_option_chain_output(data, nifty_fut_last_price);
  if (chain != 0) {
    fprintf(stderr, "Error: %s\n", "Sent earlier");
  }

  cJSON_Delete(response_json);
  cJSON_Delete(query_json);
  fyers_response_destroy(response_quotes);
}
